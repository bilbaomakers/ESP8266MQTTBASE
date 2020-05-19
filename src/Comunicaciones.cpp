
#include <Comunicaciones.h>
#include <Arduino.h>
#include <string>						// Para el manejo de cadenas
#include <AsyncMqttClient.h>			// Vamos a probar esta que es Asincrona: https://github.com/marvinroger/async-mqtt-client
#include <ArduinoJson.h>				// OJO: Tener instalada una version NO BETA (a dia de hoy la estable es la 5.13.4). Alguna pata han metido en la 6

AsyncMqttClient ClienteMQTT;

Comunicaciones::Comunicaciones(){

    strcpy(mqttserver,"127.0.0.1");
    strcpy(mqttport,"1883");
    strcpy(mqtttopic, "NOTOPIC");
    strcpy(mqttusuario, "mosquitto");
    strcpy(mqttpassword, "nopasswd");
    strcpy(mqttclientid, "noclientid");
    strcpy(SuscribeTopic, "REMOTEMCU");
    
}

void Comunicaciones::SetEventoCallback(TipoCallbackEvento ref){

    MiCallbackEventos = (TipoCallbackEvento)ref;

}

void Comunicaciones::SetMqttServidor(char* l_mqttserver){

    strcpy(mqttserver, l_mqttserver);
    this->Desonectar();
    
}

void Comunicaciones::SetMqttUsuario(char l_mqttusuario[19]){

    strcpy(mqttusuario, l_mqttusuario);
    this->Desonectar();

}

void Comunicaciones::SetMqttPassword(char l_mqttpassword[19]){

    strcpy(mqttpassword, l_mqttpassword);
    this->Desonectar();

}

void Comunicaciones::SetMqttTopic(char l_mqtttopic[33]){

    strcpy(mqtttopic, l_mqtttopic);
    this->FormaEstructuraTopics();
    this->Desonectar();

}

void Comunicaciones::SetMqttClientId(char l_mqttclientid[15]){

    strcpy(mqttclientid, l_mqttclientid);
    this->Desonectar();

}

void Comunicaciones::SetSuscribeTopic(char l_SuscribeTopic[33]){

    strcpy(SuscribeTopic, l_SuscribeTopic);
    this->Desonectar();

}

void Comunicaciones::FormaEstructuraTopics(){

    cmndTopic = "cmnd/" + String(mqtttopic) + "/#";
    statTopic = "stat/" + String(mqtttopic);
    teleTopic = "tele/" + String(mqtttopic);
    lwtTopic = teleTopic + "/LWT";
    SuscribeTeleTopic = "tele/" + String(SuscribeTopic) + "/#";
    
}

bool Comunicaciones::IsConnected(){

    return ClienteMQTT.connected();

}

void Comunicaciones::Conectar(){

    char Mensaje[100];
    //strcpy(Mensaje, "MQTT: Configurando cliente MQTT");
    //this->MiCallbackEventos(EVENTO_CONECTANDO, Mensaje);

    ClienteMQTT = AsyncMqttClient();

    this->FormaEstructuraTopics();

    ClienteMQTT.setServer(mqttserver, 1883);
    ClienteMQTT.setCleanSession(true);
    ClienteMQTT.setClientId(mqttclientid);
    ClienteMQTT.setCredentials(mqttusuario,mqttpassword);
    ClienteMQTT.setKeepAlive(4);
    ClienteMQTT.setWill(lwtTopic.c_str(),2,true,"Offline");

    // Aqui vamos a explicar por que esto, que deberia ser lo "normal" no funciona y lo que hay que hacer    
    // Esto se llama "Voy a pasar a un objeto instanciado en esta clase una funcion callback miembro, ahi queda eso"
    /*
    ClienteMQTT.onConnect(onMqttConnect);
    ClienteMQTT.onDisconnect(onMqttDisconnect);
    ClienteMQTT.onMessage(onMqttMessage);
    ClienteMQTT.onPublish(onMqttPublish);
    */
  
    // Podriamos pensar que bueno, esto o algo parecido podria funcionar porque en realidad como callback al objeto MQTT
    // yo le tengo que pasar una funcion REAL, INSTANCIADA
    // ClienteMQTT.onConnect(this->onMqttConnect);

    // Y es correcto, pero se hace asi, con std:bind (que facil eh :D estaba fumao el que invento esto jaja)
    // lo de los placeholders hace referencia a los parametros de la funcion
    ClienteMQTT.onConnect(std::bind(&Comunicaciones::onMqttConnect, this, std::placeholders::_1));
    ClienteMQTT.onDisconnect(std::bind(&Comunicaciones::onMqttDisconnect, this, std::placeholders::_1));
    ClienteMQTT.onMessage(std::bind(&Comunicaciones::onMqttMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4, std::placeholders::_5, std::placeholders::_6));
    ClienteMQTT.onPublish(std::bind(&Comunicaciones::onMqttPublish, this, std::placeholders::_1));
    
    // Y despues de todo esto conectar
    strcpy(Mensaje, ("Intentando conectar a " + String(mqttserver)).c_str());
    this->MiCallbackEventos(EVENTO_CONECTANDO, Mensaje);
    
    ClienteMQTT.connect();
    
}

void Comunicaciones::Enviar(String Topic, String Payload){

    ClienteMQTT.publish(Topic.c_str(), 2, false, Payload.c_str());

}

void Comunicaciones::Desonectar(){

    // No lanzo evento mio porque se lanza en el callback del objeto MQTT
    ClienteMQTT.disconnect();
    
}

void Comunicaciones::onMqttConnect(bool sessionPresent) {

    bool susflag = false;
	bool lwtflag = false;
	
    char Mensaje[300];

	// Suscribirse al topic de Entrada de Comandos y a los de tele y LWT del riegamatico
	if (ClienteMQTT.subscribe(cmndTopic.c_str(), 2) && ClienteMQTT.subscribe(SuscribeTeleTopic.c_str(), 2)) {
	
		susflag = true;				

	}
		
	// Publicar un Online en el LWT
	if (ClienteMQTT.publish((teleTopic + "/LWT").c_str(), 2,true,"Online")){

		lwtflag = true;

	}



	if (!susflag || !lwtflag){

		// Si falla la suscripcion o el envio del Online malo kaka. Me desconecto para repetir el proceso.
        strcpy(Mensaje, "Algo falla al suscribirme a los topics");
        MiCallbackEventos(EVENTO_DESCONECTADO, Mensaje);
		this->Desonectar();

	}

	else{

        strcpy(Mensaje, "Conectado y suscrito correctamente");
        MiCallbackEventos(EVENTO_CONECTADO, Mensaje);

	}

}

void Comunicaciones::onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {

    char Razon_Desconexion[100];

    switch (reason){

        case AsyncMqttClientDisconnectReason::MQTT_SERVER_UNAVAILABLE:

            strcpy(Razon_Desconexion, "Servidor MQTT no disponible");

        break;

        case AsyncMqttClientDisconnectReason::TCP_DISCONNECTED:

            strcpy(Razon_Desconexion, "Conexion MQTT Perdida");

        break;

        
        case AsyncMqttClientDisconnectReason::MQTT_NOT_AUTHORIZED:

            strcpy(Razon_Desconexion, "MQTT: Acceso denegado");

        break;


        default:

            strcpy(Razon_Desconexion, "Otro error de conexion MQTT sin implementar");

        break;


    }

	MiCallbackEventos(EVENTO_DESCONECTADO, Razon_Desconexion);

}

void Comunicaciones::onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  
		// Toda esta funcion me la dispara la libreria MQTT cuando recibe algo y me pasa todo lo de arriba
        // Aqui me la cocino yo a mi gusto, miro si es cmnd/#, y construyo un Json muy bonito con el comando
        // Despues a su vez yo paso este JSON mi comando MiCallbackMsgRecibido al main para que haga con ella lo que guste
        // Que generalmente sera enviarlo a la tarea de evaluacion de comandos para ver que hacer.
        String s_topic = String(topic);

		// Para que no casque si no viene payload. Asi todo OK al gestor de comandos le llega vacio como debe ser, el JSON lo pone bien.
		if (payload == NULL){

			payload = "NULL";

		}
	
		// Lo que viene en el char* payload viene de un buffer que trae KAKA, hay que limpiarlo (para eso nos pasan len y tal)
		char c_payload[len+1]; 										// Array para el payload y un hueco mas para el NULL del final
		strlcpy(c_payload, payload, len+1); 			            // Copiar del payload el tamaño justo. strcopy pone al final un NULL
		
		// Y ahora lo pasamos a String que esta limpito
		String s_payload = String(c_payload);

		// Sacamos el prefijo del topic, o sea lo que hay delante de la primera /
		int Indice1 = s_topic.indexOf("/");
		String Prefijo = s_topic.substring(0, Indice1);
		
		// Si el prefijo es cmnd o tele lo procesamos
		if (Prefijo == "cmnd" || Prefijo == "tele") { 

			// Sacamos el "COMANDO" del topic, o sea lo que hay detras de la ultima /
			int Indice2 = s_topic.lastIndexOf("/");
			String Comando = s_topic.substring(Indice2 + 1);

			DynamicJsonBuffer jsonBuffer;
			JsonObject& ObjJson = jsonBuffer.createObject();
			ObjJson.set("COMANDO",Comando);
			ObjJson.set("PAYLOAD",s_payload);

			char JSONmessageBuffer[300];
			ObjJson.printTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
            

            if (Prefijo == "cmnd"){

                MiCallbackEventos(Comunicaciones::EVENTO_CMND_RX, JSONmessageBuffer);

            }

            else if (Prefijo == "tele"){

                MiCallbackEventos(Comunicaciones::EVENTO_TELE_RX, JSONmessageBuffer);

            }
						
		}



		
}

void Comunicaciones::onMqttPublish(uint16_t packetId) {
  
	// Al publicar no hacemos nada de momento.

}

