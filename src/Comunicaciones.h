
#include <Arduino.h>
#include <string>						// Para el manejo de cadenas
#include <AsyncMqttClient.h>			// Vamos a probar esta que es Asincrona: https://github.com/marvinroger/async-mqtt-client

class Comunicaciones
{


private:

    // Esto en cristiano moderno lo vamos a llamar por su nombre. Manejadores de eventos.
    // Voy a hacer un unico manejador de eventos que pase un evento de un tipo definido aqui, y un char array con informacion
    // Este envia el comando recibido en el fomrato JSON que yo me he inventado ( {})
    typedef void(*TipoCallbackEvento)(unsigned int Evento_Comunicaciones, char Info[100]);                          // Definir el tipo (pantilla) de la funcion callback que me tendran que pasar (aqui yo tengo que saber como es aunque no la tenga aun para usarla claro)
                                                                                                         // typedef crea un alias de un tipo. En este caso crea el typo Callback Mensaje recibido que es "puntero a una funcion void X (String, String)"
    TipoCallbackEvento MiCallbackEventos = nullptr;                             // Instanaciar aqui el nuevo tipo vacio (nullptr porque recordemos que es puntero)
                                                                                // La funcion real que haga X se crea fuera de la clase y luego me pasan el puntero. Si no coincide el tipo void X (String, String) pues casca claro
                                                                   
    
    // Funciones internas para pasarle al Objeto MQTT, sus eventos ;)
    // Luego veremos que hay que hacer malabares para pasarselas
    void onMqttConnect(bool sessionPresent);
    void onMqttDisconnect(AsyncMqttClientDisconnectReason reason);
    void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
    void onMqttPublish(uint16_t packetId);
    
    // Variables internas de configuracion para el objeto MQTT
    char mqttserver[40];
	char mqttport[6];
	char mqtttopic[33];
	char mqttusuario[19];
	char mqttpassword[19];
    char mqttclientid[33];
    char SuscribeTopic[33];

    String cmndTopic;
	String statTopic;
	String teleTopic;
	String lwtTopic;
    String SuscribeTeleTopic;
   
    void FormaEstructuraTopics();


public:
    
    enum Tipo_Evento_Comunicaciones {

        EVENTO_CONECTANDO,              // Cuando iniciamos una conexion MQTT
		EVENTO_CONECTADO,				// Cuando se conecta al broker
		EVENTO_DESCONECTADO,	    	// Cuando se desconecta del Broker
		EVENTO_CMND_RX,	            	// Cuando recibo un comando
        EVENTO_TELE_RX,                 // Cuando se recibe un mensaje de Telemetria de un dispositivo remoto
		EVENTO_MSG_TX,					// Cuando se ha finalaziado la transmision de un mensaje
		
	};
    
    Comunicaciones();
    ~Comunicaciones(){};
    
    void SetMqttServidor(char* l_mqttserver);            // Configurar el servidor MQTT
    void SetMqttUsuario(char l_mqttusuario[19]);         // Configurar el usuario
    void SetMqttPassword(char l_mqttpassword[19]);       // Configurar la contraseña
    void SetMqttTopic(char l_mqtttopic[33]);             // Configurar el topic base
    void SetMqttClientId(char l_mqttclientid[33]);       // Configurar el topic base
    
    void SetSuscribeTopic(char l_SuscribeTopic[33]);      // Para las comunicaciones con riegamatico

    void SetEventoCallback(TipoCallbackEvento ref);	// Para pasarme el manejador de eventos

    void Enviar(String Topic, String Payload);

    void Conectar();
    void Desonectar();
    
    bool IsConnected();         // Para saber si esta conectado
    
    
};


