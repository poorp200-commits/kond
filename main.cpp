#define MQTTCLIENT_QOS2 1
 
#include "MQTTmbed.h"
#include "MQTTClientMbedOs.h"
#include "mbed.h"
#include "Sht31.h"

// Инициализация датчика и пина для управления реле/светодиодом
Sht31 temp_sensor(I2C_SDA, I2C_SCL);
DigitalOut relay(A0); 

int arrivedcount = 0;
float t;
float h;
int tt;
int hh;

void messageArrived(MQTT::MessageData& md)
{
    MQTT::Message &message = md.message;
    printf("Message arrived: qos %d, retained %d, dup %d, packetid %d\r\n", message.qos, message.retained, message.dup, message.id);
    printf("Payload %.*s\r\n", message.payloadlen, (char*)message.payload);
    ++arrivedcount;
}
 
void mqtt_demo(NetworkInterface *net)
{
    float version = 0.6;
    char* topic_temp = "temp";
    char* topic_hum = "hum";
 
    TCPSocket socket;
    MQTTClient client(&socket);
 
    SocketAddress a;
    char* hostname = "dev.rightech.io";
    net->gethostbyname(hostname, &a);
    int port = 1883;
    a.set_port(port);
 
    printf("Connecting to %s:%d\r\n", hostname, port);
 
    socket.open(net);
    printf("Opened socket\n\r");
    int rc = socket.connect(a);
    if (rc != 0) {
        printf("rc from TCP connect is %d\r\n", rc);
        return;
    }
    printf("Connected socket\n\r");
 
    MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
    data.MQTTVersion = 3;
    data.clientID.cstring = "mqtt-sagdatovrenat-fx525n";
    data.username.cstring = "123";
    data.password.cstring = "123";
    if ((rc = client.connect(data)) != 0) {
        printf("rc from MQTT connect is %d\r\n", rc);
        socket.close();
        return;
    }
    printf("MQTT Connected!\r\n");
 
    MQTT::Message message;
    char buf[100];

    while (1) {
        // Обрабатываем входящие сообщения перед отправкой
        client.yield(500);
        
        // Считываем данные с датчика
        t = temp_sensor.readTemperature();
        h = temp_sensor.readHumidity();
        tt = round(t);
        hh = round(h);
        printf("Sensor data -> T: %d, H: %d\n\r", tt, hh);
        
        // --- УПРАВЛЕНИЕ РЕЛЕ ---
        if (tt > 30 || hh > 50) {
            relay = 0; // Включаем реле (загорается светодиод)
            printf("Condition met (T > 30 or H > 50): Relay ON\n");
        } else {
            relay = 1; // Выключаем реле
            printf("Condition not met: Relay OFF\n");
        }
        // -----------------------
        
        // Публикуем ТЕМПЕРАТУРУ в топик "temp"
        sprintf(buf, "%d", tt);
        message.qos = MQTT::QOS0;
        message.retained = false;
        message.dup = false;
        message.payload = (void*)buf;
        message.payloadlen = strlen(buf);
        
        printf("Publishing to 'temp': %s\n", buf);
        rc = client.publish(topic_temp, message);
        if (rc != 0) {
            printf("rc from MQTT publish (temp) is %d\r\n", rc);
        }
        
        // Обрабатываем входящие сообщения
        client.yield(500);
        
        // Публикуем ВЛАЖНОСТЬ в топик "hum"
        sprintf(buf, "%d", hh);
        message.payloadlen = strlen(buf);
        
        printf("Publishing to 'hum': %s\n", buf);
        rc = client.publish(topic_hum, message);
        if (rc != 0) {
            printf("rc from MQTT publish (hum) is %d\r\n", rc);
        }
        
        // Обрабатываем входящие сообщения после публикации
        client.yield(1000);
        
        ThisThread::sleep_for(5000);
    }
 
    // Примечание: этот код недостижим из-за бесконечного цикла while(1), 
    // но он оставлен для корректного завершения, если цикл когда-либо прервется.
    if ((rc = client.unsubscribe(topic_temp)) != 0)
        printf("rc from unsubscribe was %d\r\n", rc);
 
    if ((rc = client.disconnect()) != 0)
        printf("rc from disconnect was %d\r\n", rc);
 
    socket.close();
 
    printf("Version %.2f: finish %d msgs\r\n", version, arrivedcount);
 
    return;
}

// --- Wi-Fi функции ---

WiFiInterface *wifi;

const char *sec2str(nsapi_security_t sec)
{
    switch (sec) {
        case NSAPI_SECURITY_NONE:
            return "None";
        case NSAPI_SECURITY_WEP:
            return "WEP";
        case NSAPI_SECURITY_WPA:
            return "WPA";
        case NSAPI_SECURITY_WPA2:
            return "WPA2";
        case NSAPI_SECURITY_WPA_WPA2:
            return "WPA/WPA2";
        case NSAPI_SECURITY_UNKNOWN:
        default:
            return "Unknown";
    }
}

int scan_demo(WiFiInterface *wifi)
{
    WiFiAccessPoint *ap;

    printf("Scan:\n");

    int count = wifi->scan(NULL,0);

    if (count <= 0) {
        printf("scan() failed with return value: %d\n", count);
        return 0;
    }

    count = count < 15 ? count : 15;

    ap = new WiFiAccessPoint[count];
    count = wifi->scan(ap, count);

    if (count <= 0) {
        printf("scan() failed with return value: %d\n", count);
        return 0;
    }

    for (int i = 0; i < count; i++) {
        printf("Network: %s secured: %s BSSID: %hhX:%hhX:%hhX:%hhx:%hhx:%hhx RSSI: %hhd Ch: %hhd\n", ap[i].get_ssid(),
               sec2str(ap[i].get_security()), ap[i].get_bssid()[0], ap[i].get_bssid()[1], ap[i].get_bssid()[2],
               ap[i].get_bssid()[3], ap[i].get_bssid()[4], ap[i].get_bssid()[5], ap[i].get_rssi(), ap[i].get_channel());
    }
    printf("%d networks available.\n", count);

    delete[] ap;
    return count;
}

// --- Главная функция ---

int main()
{
    printf("WiFi & MQTT Sensor example\n");

#ifdef MBED_MAJOR_VERSION
    printf("Mbed OS version %d.%d.%d\n\n", MBED_MAJOR_VERSION, MBED_MINOR_VERSION, MBED_PATCH_VERSION);
#endif

    wifi = WiFiInterface::get_default_instance();
    if (!wifi) {
        printf("ERROR: No WiFiInterface found.\n");
        return -1;
    }

    int count = scan_demo(wifi);
    if (count == 0) {
        printf("No WIFI APs found - can't continue further.\n");
        return -1;
    }

    printf("\nConnecting to %s...\n", MBED_CONF_APP_WIFI_SSID);
    int ret = wifi->connect(MBED_CONF_APP_WIFI_SSID, MBED_CONF_APP_WIFI_PASSWORD, NSAPI_SECURITY_WPA_WPA2);
    if (ret != 0) {
        printf("\nConnection error: %d\n", ret);
        return -1;
    }

    printf("Success\n\n");
    printf("MAC: %s\n", wifi->get_mac_address());
    printf("IP: %s\n", wifi->get_ip_address());
    printf("Netmask: %s\n", wifi->get_netmask());
    printf("Gateway: %s\n", wifi->get_gateway());
    printf("RSSI: %d\n\n", wifi->get_rssi());
    
    // Запуск MQTT и основного цикла обработки данных
    mqtt_demo(wifi);

    wifi->disconnect();

    printf("\nDone\n");
    return 0;
}
