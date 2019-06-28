# Presentación del trabajo final para RTOS I.

En este sistema se implementan tres tareas.

1. La primera tarea se implementa para la obtención de la hora local y el muestreo en la LCD por medio de I2C.
2. La segunda tarea permite la sincronización de la hora con un servidor remoto NTP. Levanta y cierra una conexión inalambrica.
3. La tercera tarea atiende la interrupión por UART lo que forza la actualización de la hora.

Se implemento una cola para la gestión del evento de la UART.
Se implemento un semaforo Mutex para la sincronización de las tareas, permitiendo la correcta visualización en el display en el momento que se atienda la interrupción.
