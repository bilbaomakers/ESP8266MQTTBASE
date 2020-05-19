/*
# ABONAMATICO 1.0
# Inyector de Abono en el riego con capcidades MQTT
Desarrollado con Visual Code + PlatformIO en Framework Arduino
Implementa las comunicaciones WIFI y MQTT asi como la configuracion de las mismas via comandos
Implementa el envio de comandos via puerto serie o MQTT
Implementa el uso de tareas para multiproceso con la libreria TaskScheduler
Author: Diego Maroto - BilbaoMakers 2020 - info@bilbaomakers.org - dmarofer@diegomaroto.net
https://github.com/dmarofer/ABONAMATICO_MQTT
https://bilbaomakers.org/
Licencia: GNU General Public License v3.0 - https://www.gnu.org/licenses/gpl-3.0.html
*/

#pragma region INCLUDES y DEFINES

#include <TaskScheduler.h>				// Task Scheduler
#include <cppQueue.h>					// Libreria para uso de colas.
#include <ConfigCom.h>					// Para la gestion de la configuracion de las comunicaciones.
#include <MiProyecto.h>					// Clase de Mi Proyecto
#include <ESP8266WiFi.h>				// Para la gestion de la Wifi
#include <FS.h>							// Libreria Sistema de Ficheros
#include <ArduinoJson.h>				// OJO: Tener instalada una version NO BETA (a dia de hoy la estable es la 5.13.4). Alguna pata han metido en la 6
#include <string>						// Para el manejo de cadenas
#include <NTPClient.h>					// Para la gestion de la hora por NTP
#include <WiFiUdp.h>					// Para la conexion UDP con los servidores de hora.
#include <ArduinoOTA.h>					// Actualizaciones de firmware por red.
#include <Configuracion.h>				// Fichero de configuracion
#include <Comunicaciones.h>				// Clase de Comunicaciones

#pragma endregion

#pragma region Objetos

// Manejadores Colas para comunicaciones inter-tareas
Queue ColaComandos(300, 10, IMPLEMENTATION);	// Cola para los comandos recibidos
Queue ColaTelemetria(300, 10, IMPLEMENTATION);	// Cola para la telemetria recibida
Queue ColaRespuestas(300, 10, IMPLEMENTATION);	// Cola para las respuestas a enviar

// Flag para el estado del sistema de ficheros
boolean SPIFFStatus = false;

// Conexion UDP para la hora
WiFiUDP UdpNtp;

// Manejador del NTP. Cliente red, servidor, offset zona horaria, intervalo de actualizacion.
// FALTA IMPLEMENTAR ALGO PARA CONFIGURAR LA ZONA HORARIA
static NTPClient ClienteNTP(UdpNtp, "pool.ntp.org", HORA_LOCAL * 3600, 1);

// Para el manejador de ficheros de configuracion
ConfigCom MiConfig = ConfigCom(FICHERO_CONFIG_COM);

// Para las Comunicaciones
Comunicaciones MisComunicaciones = Comunicaciones();

MiProyecto MiProyectoOBJ(FICHERO_CONFIG_PRJ, ClienteNTP);

// Task Scheduler
Scheduler MiTaskScheduler;

#pragma endregion

#pragma region Funciones de Eventos y Telemetria

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
			MisComunicaciones.Conectar();
        	break;
    	case WIFI_EVENT_STAMODE_DISCONNECTED:
        	Serial.println("Conexion WiFi: Desconetado");
        	break;
		default:
			break;

    }
		
}

// Manda a la cola de respuestas el mensaje de respuesta. Esta funcion la uso como CALLBACK para el objeto AbonaMatico
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

// Funcion ante un Evento de la libreria de comunicaciones
void EventoComunicaciones (unsigned int Evento_Comunicaciones, char Info[300]){

	
	switch (Evento_Comunicaciones)
	{
	case Comunicaciones::EVENTO_CONECTANDO:
	
		Serial.print("MQTT - CONECTANDO: ");
		Serial.println(String(Info));

	break;
	
	case Comunicaciones::EVENTO_CONECTADO:

		Serial.print("MQTT - CONECTADO: ");
		Serial.println(String(Info));
		ClienteNTP.update();
		
	break;

	case Comunicaciones::EVENTO_CMND_RX:

		Serial.print("MQTT - CMND_RX: ");
		Serial.println(String(Info));
		ColaComandos.push(Info);

	break;

	case Comunicaciones::EVENTO_TELE_RX:

		//Serial.print("MQTT - TELE_RX: ");
		//Serial.println(String(Info));
		ColaTelemetria.push(Info);

	break;

	case Comunicaciones::EVENTO_DESCONECTADO:

		

	break;

	default:
	break;

	}


}

// envia al topic tele la telemetria en Json
void MandaTelemetria() {
	

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


#pragma endregion

#pragma region TASKS

// Tarea para vigilar la conexion con el MQTT y conectar si no estamos conectados
void TaskGestionRed () {

		if (WiFi.isConnected() && !MisComunicaciones.IsConnected()){
			
			MisComunicaciones.Conectar();
			
		}
		
}

//Tarea para procesar la cola de comandos recibidos
void TaskProcesaComandos (){

	char JSONmessageBuffer[300];
				
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

							MisComunicaciones.SetMqttServidor(MiConfig.mqttserver);
							MisComunicaciones.SetMqttUsuario(MiConfig.mqttusuario);
							MisComunicaciones.SetMqttPassword(MiConfig.mqttpassword);
							MisComunicaciones.SetMqttTopic(MiConfig.mqtttopic);
							MisComunicaciones.SetMqttClientId(HOSTNAME);

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


					else if (COMANDO == "MiProyectoComando1"){

						

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

// Tarea para procesar la telemetria recibida
void TaskProcesaTelemetria(){

			char JSONmessageBuffer[300];
				
			// Limpiar el Buffer
			memset(JSONmessageBuffer, 0, sizeof JSONmessageBuffer);

			//if (xQueueReceive(ColaComandos,&JSONmessageBuffer,0) == pdTRUE ){
			if (ColaTelemetria.pull(&JSONmessageBuffer)){

				String COMANDO;
				String PAYLOAD;
				DynamicJsonBuffer jsonBuffer;
				JsonObject& ObjJson = jsonBuffer.parseObject(JSONmessageBuffer);

				if (ObjJson.success()) {
				
					COMANDO = ObjJson["COMANDO"].as<String>();
					PAYLOAD = ObjJson["PAYLOAD"].as<String>();
					
					// Procesar los mensajes de Telemetria

					if (COMANDO == "LWT"){
												
						if (PAYLOAD == "Online"){

							
						}

						else {

							
						}


					}

					else {

						//Serial.println("Me ha llegado un paquete de telemetria que no proceso.");
						//Serial.println("Comando: " + COMANDO);
						//Serial.println("Payload: " + PAYLOAD);

					}

				}

				// Y si por lo que sea la libreria JSON no puede convertir el comando recibido
				else {

						Serial.println("La tarea de procesar telemetria ha recibido un paquete JSON mal formado.");
						
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

						MisComunicaciones.Enviar(MQTTT, RESP);
						Serial.println(ClienteNTP.getFormattedTime() + " " + CMND + " " + RESP);
						
					}

					else 	if (TIPO == "MQTT"){

						MisComunicaciones.Enviar(MQTTT, RESP);
																								
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

				char JSONmessageBuffer[300];
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
void TaskAbonaMaticoRun(){


		MiProyectoOBJ.TaskRun();


}

// tarea para el envio periodico de la telemetria
void TaskMandaTelemetria(){

		
		MandaTelemetria();
		
	
}


// Definir aqui las tareas (no en SETUP como en FreeRTOS)

Task TaskProcesaComandosHandler (100, TASK_FOREVER, &TaskProcesaComandos, &MiTaskScheduler, false);
Task TaskProcesaTelemetriaHandler (1000, TASK_FOREVER, &TaskProcesaTelemetria, &MiTaskScheduler, false);
Task TaskEnviaRespuestasHandler (100, TASK_FOREVER, &TaskEnviaRespuestas, &MiTaskScheduler, false);
Task TaskAbonaMaticoRunHandler (100, TASK_FOREVER, &TaskAbonaMaticoRun, &MiTaskScheduler, false);
Task TaskMandaTelemetriaHandler (5000, TASK_FOREVER, &TaskMandaTelemetria, &MiTaskScheduler, false);
Task TaskComandosSerieRunHandler (100, TASK_FOREVER, &TaskComandosSerieRun, &MiTaskScheduler, false);
Task TaskGestionRedHandler (30000, TASK_FOREVER, &TaskGestionRed, &MiTaskScheduler, false);	

#pragma endregion

#pragma region Funcion Setup() de ARDUINO

// funcion SETUP de Arduino
void setup() {
	
	// Puerto Serie
	Serial.begin(115200);
	Serial.println();

	Serial.println("-- Iniciando Controlador AbonaMatico --");

	// Asignar funciones Callback
	MiProyectoOBJ.SetRespondeComandoCallback(MandaRespuesta);
		
	// Comunicaciones
	
	WiFi.onEvent(WiFiEventCallBack);

	// Iniciar la Wifi
	WiFi.begin();


	// Iniciar el sistema de ficheros
	SPIFFStatus = SPIFFS.begin();

	if (SPIFFS.begin()){

		Serial.println("Sistema de ficheros montado");

		// Leer la configuracion de Comunicaciones
		if (MiConfig.leeconfig()){

			// Tarea de gestion de la conexion MQTT. Lanzamos solo si conseguimos leer la configuracion
			TaskGestionRedHandler.enable();
	
		}

		// Leer configuracion salvada del Objeto MiAbonaMatico
		MiProyectoOBJ.LeeConfig();

	}

	else {

		SPIFFS.format();
		Serial.println("No se puede iniciar el sistema de ficheros, formateando ...");

	}
	
	// Configurar todo el objeto Miscomunicaciones
	MisComunicaciones.SetEventoCallback(EventoComunicaciones);
	MisComunicaciones.SetMqttServidor(MiConfig.mqttserver);
	MisComunicaciones.SetMqttUsuario(MiConfig.mqttusuario);
	MisComunicaciones.SetMqttPassword(MiConfig.mqttpassword);
	MisComunicaciones.SetMqttClientId(MiConfig.mqtttopic);
	MisComunicaciones.SetMqttTopic(MiConfig.mqtttopic);

	// TASKS
	Serial.println("Habilitando tareas del sistema.");
		
	TaskProcesaComandosHandler.enable();
	TaskProcesaTelemetriaHandler.enable();
	TaskEnviaRespuestasHandler.enable();
	TaskAbonaMaticoRunHandler.enable();
	TaskMandaTelemetriaHandler.enable();
	TaskComandosSerieRunHandler.enable();
	
	// Init Completado.
	Serial.println("Funcion Setup Completada - Tareas Scheduler y loop en marcha");


	// Iniciar Mecanica para pruebas forzando el estado a Sin Iniciar (cosa que solo se debe hacer si no esta puesta la jeringuilla)
	//Serial.println("Iniciando Mecanica desde Setup");
	//MiAbonaMatico.Estado_Mecanica = MiAbonaMatico.EM_SIN_INICIAR;
	//MiAbonaMatico.IniciaMecanica();
	
}

#pragma endregion

#pragma region Funcion Loop() de ARDUINO

// Funcion LOOP de Arduino


void loop() {

	ArduinoOTA.handle();
	//ESP.wdtFeed();
	//ClienteNTP.update();
	MiProyectoOBJ.RunFast();
	//ESP.wdtFeed();
	MiTaskScheduler.execute();
	//ESP.wdtFeed();

}

#pragma endregion

/// FIN DEL PROGRAMA ///
