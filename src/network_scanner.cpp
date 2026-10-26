#include "network_scanner.h"
#include "device.h"
#include "device_storage.h"
#include "serial_input.h"
#include "ui_protocol.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <lwip/etharp.h>
#include <lwip/netif.h>
#include <lwip/tcpip.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
namespace {
Device devices[MAX_DEVICES];
int deviceCount = 0;
// Разбор DNS-имени с ограничением длины и числа переходов по указателям.
bool dnsName(const uint8_t* data, int size, int& offset, String& name) {
  int cursor = offset;
  bool jumped = false;
  name = "";
  for (int step = 0; step < 128; ++step) {
    if (cursor >= size) return false;
    uint8_t length = data[cursor++];
    if (!length) { if (!jumped) offset = cursor; return true; }
    if ((length & 0xC0) == 0xC0) {
      if (cursor >= size) return false;
      int pointer = ((length & 0x3F) << 8) | data[cursor++];
      if (pointer >= size) return false;
      if (!jumped) offset = cursor;
      jumped = true; cursor = pointer;
      continue;
    }
    if (length > 63 || cursor + length > size || name.length() + length + 1 > 253) return false;
    if (name.length()) name += '.';
    for (int i = 0; i < length; ++i) name += char(data[cursor++]);
  }
  return false;
}
String getHostname(IPAddress ip) {
  WiFiUDP udp;

  IPAddress dns = WiFi.dnsIP();

  if (dns == IPAddress(0, 0, 0, 0)) {
    return "Unknown";
  }

  if (!udp.begin(0)) {
    return "Unknown";
  }

  uint8_t packet[128] = {};
  int pos = 0;

  uint16_t requestId = (uint16_t)random(1, 65535);

  // DNS Header
  packet[pos++] = requestId >> 8;
  packet[pos++] = requestId & 0xFF;

  packet[pos++] = 0x01;
  packet[pos++] = 0x00;

  packet[pos++] = 0x00;
  packet[pos++] = 0x01;

  // ANCOUNT, NSCOUNT, ARCOUNT
  for (int i = 0; i < 6; i++) {
    packet[pos++] = 0;
  }

  // Обратный DNS-адрес
  String query =
    String(ip[3]) + "." +
    String(ip[2]) + "." +
    String(ip[1]) + "." +
    String(ip[0]) + ".in-addr.arpa";

  int start = 0;

  while (start < query.length()) {
    int dot = query.indexOf('.', start);

    if (dot < 0) {
      dot = query.length();
    }

    int len = dot - start;

    if (pos + len + 5 >= (int)sizeof(packet)) {
      udp.stop();
      return "Unknown";
    }

    packet[pos++] = len;

    for (int i = start; i < dot; i++) {
      packet[pos++] = query[i];
    }

    start = dot + 1;
  }

  packet[pos++] = 0;

  // QTYPE PTR = 12
  packet[pos++] = 0;
  packet[pos++] = 12;

  // QCLASS IN = 1
  packet[pos++] = 0;
  packet[pos++] = 1;

  if (!udp.beginPacket(dns, 53)) {
    udp.stop();
    return "Unknown";
  }

  udp.write(packet, pos);
  if (!udp.endPacket()) { udp.stop(); return "Unknown"; }

  unsigned long started = millis();

  while (millis() - started < 300) {
    int size = udp.parsePacket();

    if (size > 0 && udp.remoteIP() == dns && udp.remotePort() == 53) {
      uint8_t response[512] = {};

      int received = udp.read(
        response,
        min(size, (int)sizeof(response))
      );

      udp.stop();

      if (received < 12 || size > (int)sizeof(response) ||
          response[0] != (requestId >> 8) || response[1] != (requestId & 0xFF) ||
          !(response[2] & 0x80) || (response[2] & 0x7A) || (response[3] & 0x0F) ||
          response[4] != 0 || response[5] != 1) return "Unknown";

      int offset = 12;
      String name;
      if (!dnsName(response, received, offset, name) || name != query || offset + 4 > received)
        return "Unknown";
      if (response[offset] != 0 || response[offset+1] != 12 ||
          response[offset+2] != 0 || response[offset+3] != 1) return "Unknown";
      offset += 4;
      int answers = (response[6] << 8) | response[7];
      for (int a = 0; a < answers; ++a) {
        if (!dnsName(response, received, offset, name) || offset + 10 > received) return "Unknown";
        int type = (response[offset] << 8) | response[offset+1];
        int klass = (response[offset+2] << 8) | response[offset+3];
        int length = (response[offset+8] << 8) | response[offset+9];
        offset += 10;
        if (offset + length > received) return "Unknown";
        if (type == 12 && klass == 1 && length > 0) {
          int cursor = offset;
          if (dnsName(response, received, cursor, name) && cursor == offset + length && name.length())
            return name;
          return "Unknown";
        }
        offset += length;
      }
      return "Unknown";
    }

    delay(5);
  }

  udp.stop();
  return "Unknown";
}

String macToString(const eth_addr* mac) {
  char buffer[18];

  snprintf(
    buffer,
    sizeof(buffer),
    "%02X:%02X:%02X:%02X:%02X:%02X",
    mac->addr[0],
    mac->addr[1],
    mac->addr[2],
    mac->addr[3],
    mac->addr[4],
    mac->addr[5]
  );

  return String(buffer);
}

void addDevice(IPAddress ip, const String& mac) {
  for (int i = 0; i < deviceCount; i++) {
    if (devices[i].mac == mac) {
      devices[i].ip = ip;
      return;
    }
  }

  if (deviceCount >= MAX_DEVICES) {
    return;
  }

  devices[deviceCount].ip = ip;
  devices[deviceCount].mac = mac;
  devices[deviceCount].name = "Unknown";

  deviceCount++;
}

void collectARP(struct netif* nif) {
  for (int i = 0; i < ARP_TABLE_SIZE; i++) {
    ip4_addr_t* foundIP = nullptr;
    struct netif* foundNetif = nullptr;
    struct eth_addr* foundMAC = nullptr;

    if (etharp_get_entry(
      i,
      &foundIP,
      &foundNetif,
      &foundMAC
    )) {
      if (foundNetif != nif) continue;

      IPAddress ip(
        ip4_addr1(foundIP),
        ip4_addr2(foundIP),
        ip4_addr3(foundIP),
        ip4_addr4(foundIP)
      );

      addDevice(ip, macToString(foundMAC));
    }
  }
}

// Все обращения к netif и ARP выполняются в потоке TCP/IP.
struct ArpCall { ip4_addr_t local; ip4_addr_t target; bool collect; bool found; SemaphoreHandle_t done; };
void arpCallback(void* arg) {
  auto& call = *static_cast<ArpCall*>(arg);
  for (netif* nif = netif_list; nif; nif = nif->next) {
    if (!ip4_addr_cmp(netif_ip4_addr(nif), &call.local) || !netif_is_up(nif)) continue;
    call.found = true;
    if (call.collect) collectARP(nif);
    else call.found = etharp_request(nif, &call.target) == ERR_OK;
    break;
  }
  xSemaphoreGive(call.done);
}
bool arpOperation(bool collect, IPAddress target = IPAddress()) {
  IPAddress local = WiFi.localIP();
  ArpCall call{};
  IP4_ADDR(&call.local, local[0], local[1], local[2], local[3]);
  IP4_ADDR(&call.target, target[0], target[1], target[2], target[3]);
  call.collect = collect;
  call.done = xSemaphoreCreateBinary();
  if (!call.done) return false;
  bool queued = tcpip_callback(arpCallback, &call) == ERR_OK;
  // Контекст на стеке должен оставаться живым до завершения callback.
  if (queued) xSemaphoreTake(call.done, portMAX_DELAY);
  vSemaphoreDelete(call.done);
  return queued && call.found;
}
} // namespace

namespace {
bool matchSavedInARP(const Device& saved, Device& found) {
  // Каждый снимок строится заново, без старых записей из предыдущего прохода.
  deviceCount = 0;
  if (!arpOperation(true)) return false;
  if (saved.mac.length()) {
    for (int i = 0; i < deviceCount; ++i) {
      if (devices[i].mac.equalsIgnoreCase(saved.mac)) {
        found = devices[i];
        return true;
      }
    }
    // При наличии MAC совпадение одного лишь имени не считается совпадением устройства.
    return false;
  }
  if (!saved.name.length() || saved.name.equalsIgnoreCase("Unknown")) return false;
  int matching = -1;
  for (int i = 0; i < deviceCount; ++i) {
    if (WiFi.status() != WL_CONNECTED) return false;
    const String name = getHostname(devices[i].ip);
    if (name.equalsIgnoreCase(saved.name)) {
      if (matching >= 0) {
        Serial.println("Имя неоднозначно: в ARP несколько устройств с таким PTR-именем.");
        return false;
      }
      matching = i;
    }
  }
  if (matching < 0) return false;
  found = devices[matching];
  return true;
}
uint32_t ipNumber(IPAddress ip) {
  uint32_t result = 0;
  for (int i = 0; i < 4; ++i) result = (result << 8) | ip[i];
  return result;
}
IPAddress ipFromNumber(uint32_t ip) {
  return IPAddress((ip >> 24) & 255, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255);
}
}

bool findSavedDevice(const Device& saved, Device& found) {
  if (WiFi.status() != WL_CONNECTED) return false;
  Serial.println("Проверка ARP: поиск сохранённого устройства...");
  if (matchSavedInARP(saved, found)) return true;

  const IPAddress localIP = WiFi.localIP();
  const IPAddress maskIP = WiFi.subnetMask();
  const uint32_t local = ipNumber(localIP), mask = ipNumber(maskIP);
  const uint32_t hosts = ~mask, network = local & mask;
  const uint32_t broadcast = network | hosts;
  if (!mask || hosts < 3 || (hosts & (hosts + 1)) || hosts - 1 > 1024) {
    Serial.println("Поиск: маска некорректна или подсеть больше 1024 хостов.");
    return false;
  }
  const auto sameNetwork = [&]() {
    return WiFi.status() == WL_CONNECTED && WiFi.localIP() == localIP && WiFi.subnetMask() == maskIP;
  };
  // Сначала обновляем ARP по последнему известному адресу.
  const uint32_t previousIP = ipNumber(saved.ip);
  if (previousIP > network && previousIP < broadcast && previousIP != local) {
    if (!arpOperation(false, saved.ip)) return false;
    delay(200);
    if (!sameNetwork()) return false;
    if (matchSavedInARP(saved, found)) return true;
  }

  Serial.println("Устройства в ARP нет; сканирование локальной сети...");
  for (uint32_t start = network + 1; start < broadcast; start += 4) {
    if (!sameNetwork()) return false;
    for (uint32_t ip = start; ip < broadcast && ip < start + 4; ++ip) {
      if (ip == local) continue;
      if (!arpOperation(false, ipFromNumber(ip))) return false;
      delay(5);
    }
    delay(150);
    if (!sameNetwork()) return false;
    if (matchSavedInARP(saved, found)) return true;
  }
  delay(300);
  return sameNetwork() && matchSavedInARP(saved, found);
}

void scanLAN() {
  if (WiFi.status() != WL_CONNECTED) { Serial.println("WiFi не подключён"); return; }
  deviceCount = 0;
  uiEvent("device_scan");

  IPAddress ip = WiFi.localIP();
  IPAddress mask = WiFi.subnetMask();

  uint32_t network = 0;
  uint32_t subnetMask = 0;

  for (int i = 0; i < 4; i++) {
    network = (network << 8) | (ip[i] & mask[i]);
    subnetMask = (subnetMask << 8) | mask[i];
  }

  uint32_t broadcast = network | ~subnetMask;

  if (broadcast <= network + 1 || broadcast - network - 1 > 1024) {
    Serial.println("Подсеть слишком большая.");
    return;
  }

  Serial.println("\nARP-сканирование...");

  const int BATCH = 4;

  for (
    uint32_t start = network + 1;
    start < broadcast;
    start += BATCH
  ) {
    for (
      uint32_t addr = start;
      addr < start + BATCH && addr < broadcast;
      addr++
    ) {
      IPAddress target(
        (addr >> 24) & 0xFF,
        (addr >> 16) & 0xFF,
        (addr >> 8) & 0xFF,
        addr & 0xFF
      );

      if (target == WiFi.localIP()) continue;

      if (!arpOperation(false, target)) {
        Serial.println("Ошибка ARP или интерфейс отключён."); return;
      }

      delay(5);
    }

    delay(150);

    if (!arpOperation(true)) return;

    delay(1);
  }

  delay(300);

  if (!arpOperation(true)) return;

  Serial.printf(
    "\nНайдено устройств: %d\n",
    deviceCount
  );

  Serial.println("\nОпределение имён...");

  for (int i = 0; i < deviceCount; i++) {
    devices[i].name = getHostname(devices[i].ip);
  }

  Serial.println("\nСписок устройств:");
  Serial.println("----------------------------------------");

  for (int i = 0; i < deviceCount; i++) {
    Serial.printf(
      "%d. %s | %s | %s\n",
      i + 1,
      devices[i].ip.toString().c_str(),
      devices[i].mac.c_str(),
      devices[i].name.c_str()
    );
    uiEvent("device", "\"index\":" + String(i + 1) + ",\"ip\":" + jsonString(devices[i].ip.toString()) +
      ",\"mac\":" + jsonString(devices[i].mac) + ",\"name\":" + jsonString(devices[i].name));
  }
}
bool selectDevice() {
  if (deviceCount == 0) {
    Serial.println("Устройства не найдены.");
    return false;
  }

  Serial.println("\nВыберите номер устройства:");
  uiRequest("device_choice");

  int choice = readTrimmedLine().toInt();

  if (choice < 1 || choice > deviceCount) {
    Serial.println("Неверный номер.");
    return false;
  }

  Device selected = devices[choice - 1];

  Serial.printf(
    "\nВыбрано: %s (%s)\n",
    selected.ip.toString().c_str(),
    selected.mac.c_str()
  );

  Serial.printf(
    "Имя: %s\n",
    selected.name.c_str()
  );

  Serial.println(
    "Введите своё имя устройства "
    "(Enter — оставить текущее):"
  );

  uiRequest("device_name");
  String customName = readTrimmedLine();

  if (customName.length() > 0) {
    selected.name = customName;
  }

  return saveDevice(selected);
}
