// ===== Firmware 2.0 - CP-03 OTA (ESP32 / Wokwi) =====
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>

const char* VERSAO_ATUAL  = "2.0";
const char* MANIFESTO_URL =
  "https://raw.githubusercontent.com/USUARIO/REPOSITORIO/main/version.json";
const int   PIN_LED = 2;

const unsigned long INTERVALO_LOG  = 3000;   // mensagens a cada 3 s
const unsigned long INTERVALO_LED  = 200;    // v2.0: LED pisca RAPIDO (200 ms)
const unsigned long INTERVALO_OTA  = 60000;  // nova consulta a cada 60 s

unsigned long ultimoLog = 0, ultimoLed = 0;
unsigned long proximaVerificacao = 10000;    // 1a consulta apos 10 s (Teste 1)
bool ledEstado = false;

// Extrai o valor de uma chave texto do JSON simples do manifesto
String extrair(const String& json, const String& chave) {
  int i = json.indexOf("\"" + chave + "\"");
  if (i < 0) return "";
  i = json.indexOf(':', i);
  int ini = json.indexOf('"', i) + 1;
  int fim = json.indexOf('"', ini);
  if (ini <= 0 || fim < 0) return "";
  return json.substring(ini, fim);
}

bool conectarWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;
  Serial.print("Conectando ao Wokwi-GUEST");
  WiFi.begin("Wokwi-GUEST", "", 6);
  unsigned long t = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Wi-Fi conectado. IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }
  return false;
}

void verificarAtualizacao() {
  Serial.println("--- Verificando atualizacao ---");
  if (!conectarWiFi()) {
    Serial.println("[ERRO] Sem conexao Wi-Fi. Mantendo versao atual.");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();   // LABORATORIO: cifra, mas NAO valida o certificado
  HTTPClient http;
  http.begin(client, MANIFESTO_URL);
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("[ERRO] Manifesto inacessivel (HTTP %d). Mantendo versao %s.\n",
                  code, VERSAO_ATUAL);
    Serial.println("Nova tentativa em 60 s.");
    http.end();
    return;
  }
  String corpo = http.getString();
  http.end();

  String disponivel = extrair(corpo, "version");
  String url        = extrair(corpo, "url");
  Serial.printf("Versao instalada: %s | Versao disponivel: %s\n",
                VERSAO_ATUAL, disponivel.c_str());

  if (disponivel == "" || url == "") {
    Serial.println("[ERRO] Manifesto invalido. Nenhuma atualizacao.");
    return;
  }
  if (disponivel == VERSAO_ATUAL) {
    Serial.println("Nenhuma atualizacao necessaria. Firmware nao sera regravado.");
    return;
  }

  Serial.println("Nova versao encontrada. Baixando firmware:");
  Serial.println(url);

  WiFiClientSecure clientBin;
  clientBin.setInsecure();
  httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  httpUpdate.setLedPin(PIN_LED, HIGH);   // LED aceso durante a gravacao
  httpUpdate.onProgress([](int atual, int total) {
    static int ultimo = -1;
    int pct = (total > 0) ? (atual * 100 / total) : 0;
    if (pct / 25 != ultimo) { ultimo = pct / 25; Serial.printf("Gravando: %d%%\n", pct); }
  });
  httpUpdate.onEnd([]() {
    Serial.println("OTA concluida com sucesso! Reiniciando...");
  });

  t_httpUpdate_return ret = httpUpdate.update(clientBin, url);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("[ERRO] OTA falhou (%d): %s\n",
                    httpUpdate.getLastError(),
                    httpUpdate.getLastErrorString().c_str());
      Serial.println("Mantendo firmware atual. Nova tentativa em 60 s.");
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("Servidor informou: sem atualizacoes.");
      break;
    case HTTP_UPDATE_OK:
      Serial.println("OTA OK.");   // em regra o ESP32 reinicia antes desta linha
      break;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);
  Serial.println();
  Serial.println("=== Firmware 2.0 iniciado (atualizado via OTA) ===");
}

void loop() {
  unsigned long agora = millis();

  if (agora - ultimoLed >= INTERVALO_LED) {      // LED rapido = versao 2.0
    ultimoLed = agora;
    ledEstado = !ledEstado;
    digitalWrite(PIN_LED, ledEstado);
  }

  if (agora - ultimoLog >= INTERVALO_LOG) {
    ultimoLog = agora;
    Serial.println("Versao 2.0");
    Serial.println("Dispositivo em operacao");
    Serial.println("Verificacao de seguranca ativa");
  }

  if (agora >= proximaVerificacao) {
    proximaVerificacao = agora + INTERVALO_OTA;
    verificarAtualizacao();
  }
}
