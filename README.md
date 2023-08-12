# Proyecto, Raspberry Pi Pico W + DHT22 + LCD 16x2 + Servidor TCP

El proyecto que desarrollé consiste en un sensor DHT22 que envía data a un LCD, este cuenta con un botón que se oprime cuando se quiera visualizar la data de manera local, también se envía la data a un servidor TCP para que este lo envié al cliente TCP cuando el cliente se conecta a dicho servidor y así ser visualizada de forma remota.

Para realizar este proyecto se implementaron los Task, Queue’s, Timers e Interrupciones de FreeRTOS
