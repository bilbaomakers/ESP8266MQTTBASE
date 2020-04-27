#ifndef _MiProyecto
#define _MiProyecto

#include <Arduino.h>
#include <NTPClient.h>					// Para la gestion de la hora por NTP
#include <WiFiUdp.h>					// Para la conexion UDP con los servidores de hora.


class MiProyecto
{



private:
    
    bool HayQueSalvar;
	String mificheroconfig;

	unsigned long t_uptime;						// Para el tiempo que llevamos en marcha

    typedef void(*RespondeComandoCallback)(String comando, String respuesta);			// Definir como ha de ser la funcion de Callback (que le tengo que pasar y que devuelve)
	RespondeComandoCallback MiRespondeComandos = nullptr;								// Definir el objeto que va a contener la funcion que vendra de fuera AQUI en la clase.

	// Para almacenar Alias (referencia) al objeto tipo NTPClient para poder usar en la clase el que viene del Main
    NTPClient &ClienteNTP;
        
    
public:

    MiProyecto(String fich_config_MiProyecto, NTPClient& ClienteNTP);
    ~MiProyecto() {};

    //  Variables Publicas
	String HardwareInfo;											// Identificador del HardWare y Software
	bool ComOK;														// Si la wifi y la conexion MQTT esta OK
	

	// Funciones Publicas
	String MiEstadoJson(int categoria);								// Devuelve un JSON con los estados en un array de 100 chars (la libreria MQTT no puede con mas de 100)
	void Run();														// Actualiza las propiedades de estado de este objeto en funcion del estado de motores y sensores
    void SetRespondeComandoCallback(RespondeComandoCallback ref);	// Definir la funcion para pasarnos la funcion de callback del enviamensajes
	boolean LeeConfig();
	boolean SalvaConfig();
    
};

#endif