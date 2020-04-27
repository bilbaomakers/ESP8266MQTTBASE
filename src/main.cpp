#pragma region COMENTARIOS

/*

Programa base para utilizar de base en tus proyectos MQTT con ESP8266
Desarrollado con Visual Code + PlatformIO en Framework Arduino
Implementa las comunicaciones WIFI y MQTT asi como la configuracion de las mismas via comandos
Implementa el envio de comandos via puerto serie o MQTT
Implementa el uso de tareas para multiproceso con la libreria TaskScheduler
Incluye la clase MiProyecto para desarrollar nuestro proyecto.
Author: Diego Maroto - BilbaoMakers 2020 - info@bilbaomakers.org - dmarofer@diegomaroto.net

https://github.com/bilbaomakers/ESP8266MQTTBASE

https://bilbaomakers.org/

Licencia: GNU General Public License v3.0 ( mas info en GitHub )

*/

#pragma endregion

#pragma region INCLUDES



// Librerias comantadas en proceso de sustitucion por la WiFiMQTTManager

#include <TaskScheduler.h>				// Task Scheduler
#include <cppQueue.h>					// Libreria para uso de colas.
#include <ConfigCom.h>					// Para la gestion de la configuracion de las comunicaciones.
#include <MiProyecto.h>					// Clase de Mi Proyecto
#include <ESP8266WiFi.h>				// Para la gestion de la Wifi
#include <AsyncMqttClient.h>			// Vamos a probar esta que es Asincrona: https://github.com/marvinroger/async-mqtt-client
#include <FS.h>							// Libreria Sistema de Ficheros
#include <ArduinoJson.h>				// OJO: Tener instalada una version NO BETA (a dia de hoy la estable es la 5.13.4). Alguna pata han metido en la 6
#include <string>						// Para el manejo de cadenas
#include <NTPClient.h>					// Para la gestion de la hora por NTP
#include <WiFiUdp.h>					// Para la conexion UDP con los servidores de hora.
#include <ArduinoOTA.h>					// Actualizaciones de firmware por red.

// Tipo de cola (lib cppQueue)
#define	IMPLEMENTATION	LIFO

// TaskScheduler options:
//#define _TASK_TIMECRITICAL    // Enable monitoring scheduling overruns
#define _TASK_SLEEP_ON_IDLE_RUN // Enable 1 ms SLEEP_IDLE powerdowns between tasks if no callback methods were invoked during the pass 
#define _TASK_STATUS_REQUEST  // Compile with support for StatusRequest functionality - triggering tasks on status change events in addition to time only
//#define _TASK_WDT_IDS         // Compile with support for wdt control points and task ids
//#define _TASK_LTS_POINTER     // Compile with support for local task storage pointer
//#define _TASK_PRIORITY          // Support for layered scheduling priority
//#define _TASK_MICRO_RES       // Support for microsecond resolutionMM
//#define _TASK_DEBUG

#pragma endregion

#pragma region Constantes y configuracion. Modificable aqui por el usuario

// Para el nombre del fichero de configuracion de comunicaciones
static const String FICHERO_CONFIG_COM = "/MiProyectoCom.json";

// Para el nombre del fichero de configuracion del proyecto
static const String FICHERO_CONFIG_PRJ = "/MiProyectoCfg.json";

// Para la zona horaria (horas de diferencia con UTC)
static const int HORA_LOCAL = 2;

#pragma endregion

#pragma region Objetos

// Para la conexion MQTT
AsyncMqttClient ClienteMQTT;

// Manejadores Colas para comunicaciones inter-tareas
//QueueHandle_t ColaComandos,ColaRespuestas;
Queue ColaComandos(100, 10, IMPLEMENTATION);	// Instantiate queue
Queue ColaRespuestas(300, 10, IMPLEMENTATION);	// Instantiate queue

// Flag para el estado del sistema de ficheros
boolean SPIFFStatus = false;

// Conexion UDP para la hora
WiFiUDP UdpNtp;

// Manejador del NTP. Cliente red, servidor, offset zona horaria, intervalo de actualizacion.
// FALTA IMPLEMENTAR ALGO PARA CONFIGURAR LA ZONA HORARIA
static NTPClient ClienteNTP(UdpNtp, "pool.ntp.org", HORA_LOCAL * 3600, 3600);

// Para el manejador de ficheros de configuracion
ConfigCom MiConfig = ConfigCom(FICHERO_CONFIG_COM);

// Objeto de la clase MiProyecto.
MiProyecto MiProyectoOBJ(FICHERO_CONFIG_PRJ, ClienteNTP);

// Task Scheduler
Scheduler MiTaskScheduler;

#pragma endregion

#pragma region Funciones de gestion de las conexiones Wifi

// Funcion ante un evento de la wifi
void WiFiEventCallBack(WiFiEvent_t event) {
    
	//Serial.printf("[WiFi-event] event: %d\n", event);
    switch(event) {
			
    	case WIFI_EVENT_STAMODE_GOT_IP:
     	   	Serial.print("Conexion WiFi: Conetado. IP: ");
      	  	Serial.println(WiFi.localIP());
			ArduinoOTA.begin();
			Serial.println("Proceso OTA arrancado.");
			ClienteNTP.begin();
        	break;
    	case WIFI_EVENT_STAMODE_DISCONNECTED:
        	Serial.println("Conexion WiFi: Desconetado");
        	break;
		default:
			break;

    }
		
}

#pragma endregion

#pragma region Funciones de gestion de las conexiones MQTT

// Manejador del evento de conexion al MQTT
void onMqttConnect(bool sessionPresent) {

	Serial.println("Conexion MQTT: Conectado");
	
	bool susflag = false;
	bool lwtflag = false;
	
	// Suscribirse al topic de Entrada de Comandos
	if (ClienteMQTT.subscribe(MiConfig.cmndTopic.c_str(), 2)) {

		// Si suscrito correctamente
		Serial.println("Suscrito al topic " + MiConfig.cmndTopic);

		susflag = true;				

	}
		
	else { Serial.println("Error Suscribiendome al topic " + MiConfig.cmndTopic); }

	
	// Publicar un Online en el LWT
	if (ClienteMQTT.publish((MiConfig.teleTopic + "/LWT").c_str(), 2,true,"Online")){

		// Si llegamos hasta aqui es estado de las comunicaciones con WIFI y MQTT es OK
		Serial.println("Publicado Online en Topic LWT: " + (MiConfig.teleTopic + "/LWT"));
		
		lwtflag = true;

	}


	if (!susflag || !lwtflag){

		// Si falla la suscripcion o el envio del Online malo kaka. Me desconecto para repetir el proceso.
		ClienteMQTT.disconnect(false);

	}

	else{

		// Si todo ha ido bien, proceso de inicio terminado.
		MiProyectoOBJ.ComOK = true;
		Serial.print("** ");
		Serial.print(ClienteNTP.getFormattedTime());
		Serial.println(" - SISTEMA INICIADO CORRECTAMENTE **");

	}

}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  
	Serial.println("Conexion MQTT: Desconectado.");

}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  
	String s_topic = String(topic);

		// Para que no casque si no viene payload. Asi todo OK al gestor de comandos le llega vacio como debe ser, el JSON lo pone bien.
		if (payload == NULL){

			payload = "NULL";

		}
	
		// Lo que viene en el char* payload viene de un buffer que trae KAKA, hay que limpiarlo (para eso nos pasan len y tal)
		char c_payload[len+1]; 										// Array para el payload y un hueco mas para el NULL del final
		strlcpy(c_payload, payload, len+1); 			// Copiar del payload el tamaño justo. strcopy pone al final un NULL
		
		// Y ahora lo pasamos a String que esta limpito
		String s_payload = String(c_payload);

		// Sacamos el prefijo del topic, o sea lo que hay delante de la primera /
		int Indice1 = s_topic.indexOf("/");
		String Prefijo = s_topic.substring(0, Indice1);
		
		// Si el prefijo es cmnd se lo mandamos al manejador de comandos
		if (Prefijo == "cmnd") { 

			// Sacamos el "COMANDO" del topic, o sea lo que hay detras de la ultima /
			int Indice2 = s_topic.lastIndexOf("/");
			String Comando = s_topic.substring(Indice2 + 1);

			DynamicJsonBuffer jsonBuffer;
			JsonObject& ObjJson = jsonBuffer.createObject();
			ObjJson.set("COMANDO",Comando);
			ObjJson.set("PAYLOAD",s_payload);

			char JSONmessageBuffer[100];
			ObjJson.printTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
			//Serial.println(String(ObjJson.measureLength()));

			// Mando el comando a la cola de comandos recibidos que luego procesara la tarea manejadordecomandos.
			//xQueueSend(ColaComandos, &JSONmessageBuffer, 0);
			ColaComandos.push(&JSONmessageBuffer);
			
		}

	//}

}

void onMqttPublish(uint16_t packetId) {
  
	// Al publicar no hacemos nada de momento.

}

// Manda a la cola de respuestas el mensaje de respuesta. Esta funcion la uso como CALLBACK para el objeto MiProyecto
void MandaRespuesta(String comando, String payload) {

			String t_topic = MiConfig.statTopic + "/" + comando;

			DynamicJsonBuffer jsonBuffer;
			JsonObject& ObjJson = jsonBuffer.createObject();
			// Tipo de mensaje (MQTT, SERIE, BOTH)
			ObjJson.set("TIPO","BOTH");
			// Comando
			ObjJson.set("CMND",comando);
			// Topic (para MQTT)
			ObjJson.set("MQTTT",t_topic);
			// RESPUESTA
			ObjJson.set("RESP",payload);

			char JSONmessageBuffer[300];
			ObjJson.printTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
			
			// Mando el comando a la cola de comandos recibidos que luego procesara la tarea manejadordecomandos.
			ColaRespuestas.push(&JSONmessageBuffer); 

}

// envia al topic tele la telemetria en Json
void MandaTelemetria() {
	
	if (ClienteMQTT.connected()){

			String t_topic = MiConfig.teleTopic + "/INFO1";

			DynamicJsonBuffer jsonBuffer;
			JsonObject& ObjJson = jsonBuffer.createObject();
			ObjJson.set("TIPO","MQTT");
			ObjJson.set("CMND","TELE");
			ObjJson.set("MQTTT",t_topic);
			ObjJson.set("RESP",MiProyectoOBJ.MiEstadoJson(1));
			
			char JSONmessageBuffer[300];
			ObjJson.printTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
			
			// Mando el comando a la cola de comandos recibidos que luego procesara la tarea manejadordecomandos.
			ColaRespuestas.push(&JSONmessageBuffer); 

	}
	
}

#pragma endregion

#pragma region TASKS

// Tarea para vigilar la conexion con el MQTT y conectar si no estamos conectados
void TaskGestionRed () {

		if (WiFi.isConnected() && !ClienteMQTT.connected()){
			
			ClienteMQTT.connect();
			
		}
		
}

//Tarea para procesar la cola de comandos recibidos
void TaskProcesaComandos (){


	char JSONmessageBuffer[100];
	

			
			// Limpiar el Buffer
			memset(JSONmessageBuffer, 0, sizeof JSONmessageBuffer);

			//if (xQueueReceive(ColaComandos,&JSONmessageBuffer,0) == pdTRUE ){
			if (ColaComandos.pull(&JSONmessageBuffer)){

				String COMANDO;
				String PAYLOAD;
				DynamicJsonBuffer jsonBuffer;

				JsonObject& ObjJson = jsonBuffer.parseObject(JSONmessageBuffer);

				if (ObjJson.success()) {
				
					COMANDO = ObjJson["COMANDO"].as<String>();
					PAYLOAD = ObjJson["PAYLOAD"].as<String>();
					
					// De aqui para abajo la retaila de comandos que queramos y lo qude han de hacer.

					// ##### COMANDOS PARA LA GESTION DE LA CONFIGURACION

					if (COMANDO == "WSsid"){
						
						String(PAYLOAD).toCharArray(MiConfig.Wssid, sizeof(MiConfig.Wssid));
						Serial.println("Wssid OK: " + PAYLOAD);

					}

					else if (COMANDO == "WPasswd"){

						String(PAYLOAD).toCharArray(MiConfig.WPasswd, sizeof(MiConfig.WPasswd));
						Serial.println("Wpasswd OK: " + PAYLOAD);
						
					}

					else if (COMANDO == "MQTTSrv"){

						String(PAYLOAD).toCharArray(MiConfig.mqttserver, sizeof(MiConfig.mqttserver));
						Serial.println("MQTTSrv OK: " + PAYLOAD);

					}

					else if (COMANDO == "MQTTUser"){

						String(PAYLOAD).toCharArray(MiConfig.mqttusuario, sizeof(MiConfig.mqttusuario));
						Serial.println("MQTTUser OK: " + PAYLOAD);

					}

					else if (COMANDO == "MQTTPasswd"){

						String(PAYLOAD).toCharArray(MiConfig.mqttpassword, sizeof(MiConfig.mqttpassword));
						Serial.println("MQTTPasswd OK: " + PAYLOAD);

					}

					else if (COMANDO == "MQTTTopic"){

						String(PAYLOAD).toCharArray(MiConfig.mqtttopic, sizeof(MiConfig.mqtttopic));
						Serial.println("MQTTTopic OK: " + PAYLOAD);

					}

					else if (COMANDO == "SaveCom"){

						if (MiConfig.escribeconfig()){

							ClienteMQTT.setServer(MiConfig.mqttserver, 1883);
							ClienteMQTT.setCredentials(MiConfig.mqttusuario,MiConfig.mqttpassword);
							ClienteMQTT.setWill(MiConfig.lwtTopic.c_str(),2,true,"Offline");
							WiFi.begin(MiConfig.Wssid, MiConfig.WPasswd);

						}
						
					}

					else if (COMANDO == "Help"){

						Serial.println("Comandos para la configuracion de las comunicaciones:");
						Serial.println("WSsid <SSID> - Configurar SSID de la Wifi");
						Serial.println("WPasswd <Contraseña> - Configurar contraseña de la Wifi ");
						Serial.println("MQTTSrv <IP|URL> - Direccion del Broker MQTT");
						Serial.println("MQTTUser <usuario> - Usuario para el Broker MQTT");
						Serial.println("MQTTPasswd <contraseña> - Contraseña para el usuario del Broker MQTT");
						Serial.println("MQTTTopic <string> - Nombre para la jerarquia de los topics MQTT");
						Serial.println("SaveCom - Salvar la configuracion en el microcontrolador");
						
					}

					// Y Ya si no es de ninguno de estos ....

					else {

						Serial.println("Me ha llegado un comando que no entiendo");
						Serial.println("Comando: " + COMANDO);
						Serial.println("Payload: " + PAYLOAD);

					}

				}

				// Y si por lo que sea la libreria JSON no puede convertir el comando recibido
				else {

						Serial.println("La tarea de procesar comandos ha recibido uno que no puede deserializar.");
						
				}
			
			}
		
	
}

// Tarea para procesar la cola de respuestas
void TaskEnviaRespuestas(){

	
	char JSONmessageBuffer[300];
	

	

		// Limpiar el Buffer
		memset(JSONmessageBuffer, 0, sizeof JSONmessageBuffer);

		if (ColaRespuestas.pull(&JSONmessageBuffer)){

				DynamicJsonBuffer jsonBuffer;

				JsonObject& ObjJson = jsonBuffer.parseObject(JSONmessageBuffer);

				if (ObjJson.success()) {
					
					String TIPO = ObjJson["TIPO"].as<String>();
					String CMND = ObjJson["CMND"].as<String>();
					String MQTTT = ObjJson["MQTTT"].as<String>();
					String RESP = ObjJson["RESP"].as<String>();
					
					if (TIPO == "BOTH"){

						ClienteMQTT.publish(MQTTT.c_str(), 2, false, RESP.c_str());
						Serial.println(ClienteNTP.getFormattedTime() + " " + CMND + " " + RESP);
						
					}

					else 	if (TIPO == "MQTT"){

						ClienteMQTT.publish(MQTTT.c_str(), 2, false, RESP.c_str());
																								
					}
					
					else 	if (TIPO == "SERIE"){

							Serial.println(ClienteNTP.getFormattedTime() + " " + CMND + " " + RESP);
						
					}
						
				}
		}

	
}


// Esto aqui fuera porque este scheduler es distinto que FreeRTOS. La funcion de tarea se crea y destruye cada ejecucion.
char sr_buffer[120];
int16_t sr_buffer_len(sr_buffer!=NULL && sizeof(sr_buffer) > 0 ? sizeof(sr_buffer) - 1 : 0);
int16_t sr_buffer_pos = 0;
char* sr_term = "\r\n";
char* sr_delim = " ";
int16_t sr_term_pos = 0;
char* sr_last_token;

// Tarea para los comandos que llegan por el puerto serie
void TaskComandosSerieRun(){


	char* comando = "NA";
	char* parametro1 = "NA";

		while (Serial.available()) {

			// leer un caracter del serie (en ASCII)
			int ch = Serial.read();

			

			// Si es menor de 0 es KAKA
			if (ch <= 0) {
				
				continue;
			
			}

			// Si el buffer no esta lleno, escribir el caracter en el buffer y avanzar el puntero
			if (sr_buffer_pos < sr_buffer_len){
			
				sr_buffer[sr_buffer_pos++] = ch;
				//Serial.println("DEBUG: " + String(sr_buffer));
				

			}
		
			// Si esta lleno ........
			else { 

				return;

			}

			// Aqui para detectar el retorno de linea
			if (sr_term[sr_term_pos] != ch){
				sr_term_pos = 0;
				continue;
			}

			// Si hemos detectado el retorno de linea .....
			if (sr_term[++sr_term_pos] == 0){

				sr_buffer[sr_buffer_pos - strlen(sr_term)] = '\0';

				// Aqui para sacar cada una de las "palabras" del comando que hemos recibido con la funcion strtok_r (curiosa funcion)
				comando = strtok_r(sr_buffer, sr_delim, &sr_last_token);
				parametro1 = strtok_r(NULL, sr_delim, &sr_last_token);

				// Formatear el JSON del comando y mandarlo a la cola de comandos.
				DynamicJsonBuffer jsonBuffer;
				JsonObject& ObjJson = jsonBuffer.createObject();
				ObjJson.set("COMANDO",comando);
				ObjJson.set("PAYLOAD",parametro1);

				char JSONmessageBuffer[100];
				ObjJson.printTo(JSONmessageBuffer, sizeof(JSONmessageBuffer));
			
				// Mando el comando a la cola de comandos recibidos que luego procesara la tarea manejadordecomandos.
				Serial.println(JSONmessageBuffer);
				ColaComandos.push(&JSONmessageBuffer);
				
				// Reiniciar los buffers
				sr_buffer[0] = '\0';
				sr_buffer_pos = 0;
				sr_term_pos = 0;
				
			}
		

		}
		
	
}

// Tarea para el metodo run del objeto de la cupula.
void TaskMiProyectoRun(){


		MiProyectoOBJ.Run();


}

// tarea para el envio periodico de la telemetria
void TaskMandaTelemetria(){

		
		MandaTelemetria();
		
		
	
}


// Definir aqui las tareas (no en SETUP como en FreeRTOS)

Task TaskProcesaComandosHandler (100, TASK_FOREVER, &TaskProcesaComandos, &MiTaskScheduler, false);
Task TaskEnviaRespuestasHandler (100, TASK_FOREVER, &TaskEnviaRespuestas, &MiTaskScheduler, false);
Task TaskMiProyectoRunHandler (100, TASK_FOREVER, &TaskMiProyectoRun, &MiTaskScheduler, false);
Task TaskMandaTelemetriaHandler (5000, TASK_FOREVER, &TaskMandaTelemetria, &MiTaskScheduler, false);
Task TaskComandosSerieRunHandler (100, TASK_FOREVER, &TaskComandosSerieRun, &MiTaskScheduler, false);
Task TaskGestionRedHandler (4000, TASK_FOREVER, &TaskGestionRed, &MiTaskScheduler, false);	



#pragma endregion

#pragma region Funcion Setup() de ARDUINO

// funcion SETUP de Arduino
void setup() {
	
	// Puerto Serie
	Serial.begin(115200);
	Serial.println();

	Serial.println("-- Iniciando Controlador MiProyecto --");

	// Asignar funciones Callback
	MiProyectoOBJ.SetRespondeComandoCallback(MandaRespuesta);
		
	// Comunicaciones
	ClienteMQTT = AsyncMqttClient();
	WiFi.onEvent(WiFiEventCallBack);

	// Iniciar la Wifi
	WiFi.begin();

	//MiTaskScheduler.init();

	// Iniciar el sistema de ficheros
	SPIFFStatus = SPIFFS.begin();

	if (SPIFFS.begin()){

		Serial.println("Sistema de ficheros montado");

		// Leer la configuracion de Comunicaciones
		if (MiConfig.leeconfig()){

			// Las funciones callback de la libreria MQTT	
			ClienteMQTT.onConnect(onMqttConnect);
  			ClienteMQTT.onDisconnect(onMqttDisconnect);
  			ClienteMQTT.onMessage(onMqttMessage);
  			ClienteMQTT.onPublish(onMqttPublish);
  			ClienteMQTT.setServer(MiConfig.mqttserver, 1883);
			ClienteMQTT.setCleanSession(true);
			ClienteMQTT.setClientId("MiProyecto");
			ClienteMQTT.setCredentials(MiConfig.mqttusuario,MiConfig.mqttpassword);
			ClienteMQTT.setKeepAlive(4);
			ClienteMQTT.setWill(MiConfig.lwtTopic.c_str(),2,true,"Offline");

			// Tarea de gestion de la conexion MQTT. Lanzamos solo si conseguimos leer la configuracion

			TaskGestionRedHandler.enable();
	
		}

		// Leer configuracion salvada del Objeto MiProyectoOBJ
		MiProyectoOBJ.LeeConfig();

	}

	else {

		SPIFFS.format();
		Serial.println("No se puede iniciar el sistema de ficheros, formateando ...");

	}
	
	// TASKS
	Serial.println("Habilitando tareas del sistema.");
		
	TaskProcesaComandosHandler.enable();
	TaskEnviaRespuestasHandler.enable();
	TaskMiProyectoRunHandler.enable();
	TaskMandaTelemetriaHandler.enable();
	TaskComandosSerieRunHandler.enable();
	
	// Init Completado.
	Serial.println("Setup Completado.");
	
}

#pragma endregion

#pragma region Funcion Loop() de ARDUINO

// Funcion LOOP de Arduino


void loop() {

	ArduinoOTA.handle();
	MiTaskScheduler.execute();
	ClienteNTP.update();

}

#pragma endregion

/// FIN DEL PROGRAMA ///
