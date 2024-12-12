Eu amo a minha mãe
#include <WiFi.h>
#include <HTTPClient.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <SD_MMC.h>
#include <SD.h>
#include <SPI.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

// Defina o caminho para o arquivo no cartão SD
const char* filename = "/config.txt";

// Variáveis para armazenar os dados lidos
struct WiFiCredentials {
  String ssid;
  String password;
};
std::vector<WiFiCredentials> wifiCredentialsList;
String serverUrl;

#define GPS_RX_PIN 12  // Define o pino RX ao qual o módulo GPS está conectado
#define GPS_TX_PIN 13  // Define o pino TX ao qual o módulo GPS está conectado

TinyGPSPlus gps;  // Cria um objeto da classe TinyGPSPlus para processar os dados de GPS
HardwareSerial gpsSerial(1);  // Cria um objeto da classe HardwareSerial para a comunicação com o módulo GPS (Serial1 no ESP32)

const long utcOffsetInSeconds = -3 * 3600; // Fuso horário (GMT-3)

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", utcOffsetInSeconds);

String printDateAndTime(unsigned long epochTime) {
  // Número de segundos em um dia
  const unsigned long SECONDS_PER_DAY = 86400;
  // Número de segundos em uma hora
  const unsigned long SECONDS_PER_HOUR = 3600;
  // Número de segundos em um minuto
  const unsigned long SECONDS_PER_MINUTE = 60;

  // Calcula os componentes da data e hora
  int days = epochTime / SECONDS_PER_DAY;
  int hours = (epochTime % SECONDS_PER_DAY) / SECONDS_PER_HOUR;
  int minutes = (epochTime % SECONDS_PER_HOUR) / SECONDS_PER_MINUTE;
  int seconds = epochTime % SECONDS_PER_MINUTE;

  // Calcula a data (simplesmente, sem considerar anos bissextos ou meses diferentes)
  int year = 1970;
  while (days >= 365) {
    if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
      if (days >= 366) {
        days -= 366;
      } else {
        break;
      }
    } else {
      days -= 365;
    }
    year++;
  }
  int month = 0;
  int day = 0;
  static const int monthDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  for (month = 0; month < 12; month++) {
    int daysInMonth = monthDays[month];
    if (month == 1 && ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0))) {
      daysInMonth = 29;
    }
    if (days < daysInMonth) {
      break;
    }
    days -= daysInMonth;
  }
  day = days + 1;

  // Constrói a string com a data e a hora
  String formattedDateTime = String(year) + "-" + (month + 1 < 10 ? "0" : "") + String(month + 1) + "-" + (day < 10 ? "0" : "") + String(day) + "T" + (hours < 10 ? "0" : "") + String(hours) + ":" + (minutes < 10 ? "0" : "") + String(minutes) + ":" + (seconds < 10 ? "0" : "") + String(seconds);
  
  return formattedDateTime;
}

int registro = 1; // Variável para armazenar o número do registro inicial

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  // Inicializar o módulo SD
  if (!SD_MMC.begin("/sdcard", true)) {
    Serial.println("Falha ao inicializar o cartão SD!");
    return;
  }
  Serial.println("Cartão SD inicializado com sucesso!");

  // Abre o arquivo para leitura
  File file = SD_MMC.open(filename);
  if (!file) {
    Serial.println("Falha ao abrir o arquivo");
    return;
  }

  // Lê os dados do arquivo
  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim(); // Remove espaços em branco

    // Verifica o tipo de dado e armazena nas variáveis correspondentes
    if (line.startsWith("ssid:")) {
      int commaIndex = line.indexOf(',');
      if (commaIndex > 5) {
        WiFiCredentials creds;
        creds.ssid = line.substring(5, commaIndex);
        creds.password = line.substring(commaIndex + 10);
        wifiCredentialsList.push_back(creds);
      }
    } else if (line.startsWith("serverUrl:")) {
      serverUrl = line.substring(10);
    }
  }
  file.close();

  // Conectar à rede Wi-Fi
  Serial.println("Conectando às redes Wi-Fi...");
  for (const auto& creds : wifiCredentialsList) {
    Serial.print("Tentando conectar-se a ");
    Serial.println(creds.ssid);
    WiFi.begin(creds.ssid.c_str(), creds.password.c_str());
    delay(5000);
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nConectado à rede Wi-Fi");
      break;
    } else {
      Serial.println("\nFalha na conexão à rede Wi-Fi");
    }
  }

  // Inicializar a comunicação serial com o módulo GPS
  gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  
 // Iniciar cliente NTP
  timeClient.begin();
}
void loop() {
  if (Serial.available() > 0) { // Verifica se há dados disponíveis para leitura no Serial Monitor
    String input = Serial.readStringUntil('\n'); // Lê a entrada do Serial Monitor até encontrar uma quebra de linha
    if (input == "gps") {
      // Se a entrada for "gps", obter a latitude e longitude
      getGPSData();
    } else if (input == "sdget") {
      // Se a entrada for "sdget", ler os dados do arquivo no cartão SD
      readSDData();
    } else if (input == "sddel") {
      // Se a entrada for "sddel", apagar o conteúdo do arquivo do cartão SD
      clearSDFile();
    } else if (input == "sdsend") {
      // Se a entrada for "sdsend", enviar o conteúdo do arquivo do cartão SD
      verificarEnvioCartaoSD();
    } else if (input == "sdreg") {
      // Se a entrada for "sdreg", obter dados da planilha
      fetchData();
    } else if (input == "sdenvi") {
      // Se a entrada for "sdsend", enviar o conteúdo do arquivo do cartão SD
      verificarEnvioCartaoSD();

    }else if (input == "sdadd") {
      // Se a entrada for "sdadd", adicionar dados ao cartão SD
      saveToSD("-21.258810", "-46.141857", "2024-06-13T10:20:03");
    } else if (input == "sdsendd") {
      // Se a entrada for "sdsendd", enviar dados específicos para Google Sheets
      int enviado = sendToGoogleSheets("1","-21.258810", "-46.141857", "2024-06-13T10:20:03");
      Serial.println("Resultado do envio");
      Serial.println(enviado);
    }
  }

  // Atualizar o cliente NTP
  timeClient.update();

  // Obtém o timestamp Unix atual
  unsigned long epochTime = timeClient.getEpochTime();
  
  // Imprime a data e a hora
  Serial.print("Current time2: ");
  // Obter a data e hora atual
  String datetime = printDateAndTime(epochTime);

  Serial.print("Data e Hora (Epoch): ");
  Serial.println(datetime);



  // Processar os dados de GPS
  smartdelay_gps(3000);

  // Verificar se há uma localização válida disponível
  if (gps.location.isValid()) {
    // Extrair a latitude e longitude da localização atual
    float latitude = gps.location.lat();
    float longitude = gps.location.lng();

    // // Obter a data e hora atual do NTP
    // String date_time = timeClient.getFormattedTime();
    // Obter a data e hora atual
    unsigned long epochTime = timeClient.getEpochTime();
    String date_time = printDateAndTime(epochTime);

    // Formatar latitude e longitude
    char latStr[15]; // Tamanho aumentado para acomodar os espaços e o ponto decimal
    char lonStr[15]; // Tamanho aumentado para acomodar os espaços e o ponto decimal
    dtostrf(latitude, 10, 6, latStr); // Adicionado mais espaço para a parte inteira e o ponto decimal
    dtostrf(longitude, 10, 6, lonStr); // Adicionado mais espaço para a parte inteira e o ponto decimal

    // Imprimir os valores formatados
    Serial.print("Latitude: ");
    Serial.println(latitude, 6); // A impressão com precisão definida evita espaços indesejados
    Serial.print("Longitude: ");
    Serial.println(longitude, 6); // A impressão com precisão definida evita espaços indesejados
    Serial.print("HDOP: ");
    Serial.println(gps.hdop.value());

    // Calcular precisão aproximada em metros
    double hdop = gps.hdop.hdop();
    double precision = hdop * 5; // Aproximadamente 5 metros por unidade de HDOP
    Serial.print("Precision (m): ");
    Serial.println(precision);

    Serial.print("Data e Hora: ");
    Serial.println(date_time);

    if (precision <= 7) {
      Serial.println("precisao aceitavel");
      Serial.println(precision);
      saveToSD(latStr, lonStr, date_time);
    }else{
      Serial.println("precisão Nao aceitavél");
      Serial.println(precision);
    }

    delay(5000);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Dados enviados ao Google ");
    verificarEnvioCartaoSD();
  }
  delay(40000);
}

/// #####################   FUNCOES  ##########################################

int sendToGoogleSheets(String registro,String latitudeStr, String longitudeStr, String dateStr) {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("Dados enviados ao Google Sheets: ");

    // Adiciona aspas simples às coordenadas para formatar corretamente
    latitudeStr = "'" + latitudeStr;
    longitudeStr = "'" + longitudeStr;

    Serial.print("Registro: ");
    Serial.println(registro);
    
    Serial.print("Latitude: ");
    Serial.println(latitudeStr);

    Serial.print("Longitude: ");
    Serial.println(longitudeStr);

    Serial.print("Data: ");
    Serial.println(dateStr);
    
    HTTPClient http;
    
    // Defina os parâmetros
    String nome = "esp32-cam";
    // Obter a data e hora atual
    unsigned long epochTime = timeClient.getEpochTime();
    String datetime = printDateAndTime(epochTime);

    Serial.print("Data e Hora (Epoch): ");
    Serial.println(datetime);

    //String datetime = "2024-06-07T12:34:56"; // Exemplo de data e hora completa
    //String registro = "1"; // Exemplo de registro
    
    // Construa a URL
    String url = String(serverUrl) + "?action=Create";
    url += "&nome=" + nome;
    url += "&date=" + dateStr;
    url += "&datetime=" + dateStr;
    url += "&lon=" + longitudeStr;
    url += "&lat=" + latitudeStr;
    url += "&registro=" + registro;

    Serial.println("URL: " + url);

    Serial.println("Enviando coordenadas para Google Sheets...");
    http.begin(url);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); // Seguir redirecionamentos

    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.printf("HTTP code: %d\n", httpCode);
      if (httpCode == 200) {
        Serial.println("Dados enviados ao Google Sheets com sucesso!");
        http.end();
        return 1; // Sucesso
      } else {
        Serial.println("Falha ao enviar dados ao Google Sheets");
      }
    } else {
      Serial.printf("Falha na solicitação HTTP, código: %d\n", httpCode);
    }
    http.end();
    return 0; // Falha
  } else {
    Serial.println("WiFi não está conectado");
    return 0; // Falha
  }
}

void saveToSD(String latitudeStr, String longitudeStr, String dateStr) {
  int registro = 0;

  // Abrir o arquivo data.txt para leitura e determinar o último registro
  File dataFile = SD_MMC.open("/data.txt", FILE_READ);
  if (dataFile) {
    while (dataFile.available()) {
      String line = dataFile.readStringUntil('\n');
      if (line.startsWith("Registro: ")) {
        registro = line.substring(10).toInt() + 1;
      }
    }
    dataFile.close(); // Fechar o arquivo de leitura
  } else {
    Serial.println("Erro ao abrir o arquivo para buscar registro 'data.txt'!");
  }

  // Abrir o arquivo data.txt para apêndice e escrever os novos dados
  dataFile = SD_MMC.open("/data.txt", FILE_APPEND);
  if (dataFile) {
    dataFile.print("Registro: ");
    dataFile.println(registro);
    dataFile.print("Latitude: ");
    dataFile.println(latitudeStr);
    dataFile.print("Longitude: ");
    dataFile.println(longitudeStr);
    dataFile.print("Data: ");
    dataFile.println(dateStr);
    dataFile.print("Enviado: ");
    dataFile.println("nao");
    dataFile.println();
    dataFile.close();
    Serial.println("Dados gravados no arquivo data.txt!");
  } else {
    Serial.println("Erro ao abrir o arquivo 'data.txt' para escrita!");
  }

  // Resetar o registro para data2.txt
  registro = 0;

  // Abrir o arquivo data2.txt para leitura e determinar o último registro
  File dataFile2 = SD_MMC.open("/data2.txt", FILE_READ);
  if (dataFile2) {
    while (dataFile2.available()) {
      String line = dataFile2.readStringUntil('\n');
      if (line.startsWith("Registro: ")) {
        registro = line.substring(10).toInt() + 1;
      }
    }
    dataFile2.close(); // Fechar o arquivo de leitura
  } else {
    Serial.println("Erro ao abrir o arquivo para buscar registro 'data2.txt'!");
  }

  // Abrir o arquivo data2.txt para apêndice e escrever os novos dados
  dataFile2 = SD_MMC.open("/data2.txt", FILE_APPEND);
  if (dataFile2) {
    dataFile2.print("Registro: ");
    dataFile2.println(registro);
    dataFile2.print("Latitude: ");
    dataFile2.println(latitudeStr);
    dataFile2.print("Longitude: ");
    dataFile2.println(longitudeStr);
    dataFile2.print("Data: ");
    dataFile2.println(dateStr);
    dataFile2.print("Enviado: ");
    dataFile2.println("nao");
    dataFile2.println();
    dataFile2.close();
    Serial.println("Dados gravados no arquivo data2.txt!");
  } else {
    Serial.println("Erro ao abrir o arquivo 'data2.txt' para escrita!");
  }
}



void getGPSData() {
  // Verificar se há uma localização válida disponível
  if (gps.location.isValid()) {
    // Extrair a latitude e longitude da localização atual
    float latitude = gps.location.lat();
    float longitude = gps.location.lng();

    // Imprimir os dados no Serial
    Serial.print("Latitude: ");
    Serial.println(latitude, 6);
    Serial.print("Longitude: ");
    Serial.println(longitude, 6);
  } else {
    Serial.println("Nenhuma localização válida disponível!");
  }
}

void readSDData() {
  // Abrir o arquivo para leitura
  File dataFile = SD_MMC.open("/data.txt", FILE_READ);
  if (dataFile) {
    // Ler e imprimir o conteúdo do arquivo
    Serial.println("Conteúdo do arquivo:");
    while (dataFile.available()) {
      Serial.write(dataFile.read());
    }
    dataFile.close(); // Fechar o arquivo após ler
  } else {
    Serial.println("Erro ao abrir o arquivo 'data.txt'!");
  }
}

void clearSDFile() {
  // Verificar se o arquivo existe antes de tentar apagá-lo
  if (SD_MMC.exists("/data.txt")) {
    // Abre o arquivo em modo de leitura e escrita para limpar seu conteúdo
    File dataFile = SD_MMC.open("/data.txt", FILE_WRITE);
    if (dataFile) {
      // Fecha o arquivo
      dataFile.close();
      // Reabre o arquivo em modo de escrita, o que limpa seu conteúdo
      dataFile = SD_MMC.open("/data.txt", FILE_WRITE);
      dataFile.close();
      Serial.println("Arquivo 'data.txt' foi limpo com sucesso.");
    } else {
      Serial.println("Erro ao abrir o arquivo 'data.txt' para escrita.");
    }
  } else {
    Serial.println("Erro: O arquivo 'data.txt' não existe.");
  }
}

void verificarEnvioCartaoSD() {
  // Abre o arquivo no cartão SD para leitura
  File dataFile = SD_MMC.open("/data.txt", FILE_READ);
  if (!dataFile) {
    Serial.println("Falha ao abrir o arquivo para leitura");
    return;
  }

  // Lê os dados do arquivo
  String conteudo = "";
  while (dataFile.available()) {
    conteudo += dataFile.readStringUntil('\n') + "\n";
  }
  dataFile.close();

  // Divide o conteúdo em registros
  int pos = 0;
  while ((pos = conteudo.indexOf("Registro: ", pos)) != -1) {
    int fimRegistro = conteudo.indexOf("Registro: ", pos + 1);
    if (fimRegistro == -1) fimRegistro = conteudo.length(); // Último registro pode não ter um próximo "Registro: "
    String registro = conteudo.substring(pos, fimRegistro);

    // Verifica se a linha indica que o registro já foi enviado
    if (registro.indexOf("Enviado: nao") != -1) {
      // Extrai dados do registro
      int index1 = registro.indexOf("Registro: ") + 10;
      int index2 = registro.indexOf("\n", index1);
      String registroNum = registro.substring(index1, index2);
      registroNum.trim();

      int index3 = registro.indexOf("Latitude: ") + 10;
      int index4 = registro.indexOf("\n", index3);
      String latitude = registro.substring(index3, index4);
      latitude.trim();

      int index5 = registro.indexOf("Longitude: ") + 11;
      int index6 = registro.indexOf("\n", index5);
      String longitude = registro.substring(index5, index6);
      longitude.trim();

      int index7 = registro.indexOf("Data: ") + 6;
      int index8 = registro.indexOf("\n", index7);
      String data = registro.substring(index7, index8);
      data.trim();

      // Converter latitude e longitude para valores numéricos
      double lat = latitude.toDouble();
      double lon = longitude.toDouble();

      // Verificação de sucesso da conversão
      if (lat == 0.0 || lon == 0.0) {
        Serial.println("Falha na conversão de latitude ou longitude");
        continue;
      }

      // Formatar latitude, longitude e número do registro
      char latStr[15]; // Tamanho adequado para acomodar os valores formatados
      char lonStr[15]; // Tamanho adequado para acomodar os valores formatados
      char regStr[15]; // Tamanho adequado para acomodar os valores formatados
      dtostrf(lat, 10, 6, latStr); // Formatar latitude
      dtostrf(lon, 10, 6, lonStr); // Formatar longitude
      dtostrf(registroNum.toDouble(), 10, 0, regStr); // Formatar número do registro

      int enviado = sendToGoogleSheets(registroNum, latStr, lonStr, data);
      if (enviado) {
        // Marcar como enviado
        registro.replace("Enviado: nao", "Enviado: sim");

        // Substituir no conteúdo total
        conteudo = conteudo.substring(0, pos) + registro + conteudo.substring(fimRegistro);
      } else {
        Serial.println("Falha ao enviar dados para Google Sheets");
      }
    }
    
    // Atualizar pos para próxima busca
    pos = fimRegistro;
  }

  // Salvar o conteúdo atualizado de volta no arquivo
  dataFile = SD_MMC.open("/data.txt", FILE_WRITE);
  if (dataFile) {
    Serial.println("Salvando conteudo  dentro do aquivo ");
    dataFile.print(conteudo);
    dataFile.close();
  } else {
    Serial.println("Falha ao abrir o arquivo para escrita");
  }
  clearSDFile();
}


void fetchData() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);

    int httpResponseCode = http.GET();
    if (httpResponseCode > 0) {
      String payload = http.getString();
      Serial.println("Dados da planilha:");
      Serial.println(payload);
      
      // Processando o JSON
      const size_t capacity = JSON_OBJECT_SIZE(1) + 20;
      DynamicJsonDocument doc(capacity);
      
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        const char* registro = doc["registro"];
        if (registro) {
          Serial.println("Registro:");
          Serial.println(registro);
        } else {
          Serial.println("Campo 'registro' ausente ou inválido no JSON.");
        }
      } else {
        Serial.print("Erro ao analisar JSON: ");
        Serial.println(error.c_str());
      }
    } else {
      Serial.print("Erro na solicitação HTTP: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("Erro ao conectar ao WiFi");
  }
}

static void smartdelay_gps(unsigned long ms) {
  unsigned long start = millis();  // Obtém o tempo atual em milissegundos
  do {
    while (gpsSerial.available())  // Verifica se há dados disponíveis na porta serial do módulo GPS
      gps.encode(gpsSerial.read());  // Lê os dados do GPS e os decodifica usando a biblioteca TinyGPS++
  } while (millis() - start < ms);  // Aguarda até que o tempo especificado tenha passado
}
