==== INSTRUCCIONES DE USO ====

1. Abrir la carpeta donde haya guardado nuestra tarea (vía cd o el método correspondiente según su sistema operativo —que no es Windows).

2. Ejecutar el makefile con el comando "make".

3. Abrir el ejecutable con el comando "make run" o "g++ -Wall -Wextra -std=c++17 -pthread main.cpp -o grid_tarea_2_cristobal_maria" y "./grid_tarea_2_cristobal_maria".

4. Elegir la simulación (ejemplo_1, ejemplo_2, ejemplo_3, 15x15 o 30x30).

5. Elegir la preferencia de simbolización (solo colores o símbolos + colores).

6. Elegir si se borra o se mantiene el grid (recomiendo mantenerlo para ver los ultimos movimientos).

7. Evaluar, con piedad.

==== FUNCIONAMIENTO DEL CÓDIGO ====

- Consiste en un input de texto (preparado previamente) que define los rasgos iniciales de monstruos y héroes, los cuales son representados con colores o símbolos (⁺ o ⁻) en la terminal.

- Los héroes y monstruos tienen cada uno rasgos propios, y en el caso de los héroes, también un path individual.

- Los monstruos generan su path óptimo hacia los héroes dentro de su rango de visión, recalculándolo en cada movimiento. Es decir, si mientras siguen al héroe 1, el héroe 2 resulta estar más cerca, cambiarán su objetivo hacia el héroe 2.

- Cada monstruo genera su path utilizando el algoritmo Manhattan, priorizando el eje X.

- La prioridad de acciones es la siguiente: detección → alerta → generación de path (si es necesario) → atacar/moverse (dependiendo de si está dentro de su rango de ataque).

- La alerta se basa en la última posición conocida del héroe al ser detectado. Si este se mueve, los monstruos irán hacia la posición desactualizada, ya que no tienen forma de corregir su path hasta volver a verlo. En cambio, si el héroe se detiene para atacar y vuelve a entrar en el rango de visión de los monstruos, estos actualizan su path inmediatamente.

- La simulación finaliza cuando todos los héroes mueren o cuando los que sigan vivos completan su path.

- Todos los cálculos ignoran héroes o monstruos que estén muertos o que hayan finalizado su path (en el caso de los héroes).

- Si un héroe se encuentra con otro que ya haya finalizado su recorrido, simplemente pasará por encima, y en el grid se mostrará el que tenga un ID menor (se intentó implementar que se vieran ambos o solo el que sigue caminando, pero no hubo tiempo).

- Cada entidad (héroe o monstruo) tiene su propio archivo de log individual (log_heroes/heroe_[id].log y log_monstruos/monstruo_[id].log), donde se registran sus acciones.

- El grid se actualiza visualmente dos veces por ciclo, mostrando los estados en colores o símbolos, según la preferencia del usuario.

- Se incluye protección ante deadlocks y acceso concurrente mediante try_to_lock en las secciones críticas.

- MUTEX

- - mtx_grid: bloquea el grid para que las entidades puedan moverse sin chocar o ocupar el mismo cuadro simultáneamente.

- - mtx_combate: garantiza que no se realicen dos ataques simultáneos, respetando el “turno” de ataque. Además, si un héroe o monstruo muere justo antes de atacar, se cancela su acción.

- - mtx_log: mutex para cada monstruo y heroe, así los logs no se traspapelan. Teniendo en cuenta que otros threads tambien hacen logs dentro de cada monstruo/heroe.

- - mtx_pasos: mutex para cada monstruo, la funcion alertar genera pasos de un hilo a otro, para que no se topen entre si se utiliza este mutex, si no, se producen "segmentation faults".

- En caso de quedar atascados por cualquier motivo, las entidades esperan un máximo de 3 turnos y luego se ignora el problema de atasco (la impresión tiene prioridad por ID, mostrando primero a los héroes).

- El tiempo entre ciclos se define con la variable "tfinal", introducida por el usuario al inicio de la simulación.

- Puede que visualmente se vean movimientos en diagonal, revisar logs en ese caso, ya que depende del orden de ejecucion, el tiempo usado en la impresion del grid puede descordinarse con el timepo por ciclo de cada loop (monstruos/heroes).

- Los ataques de heroes se hacen 0.0000001 segundos mas rapido que los de los monstruos, sino siempre pierden los heroes. (nerf a monstruos...)

- EN CASO DE QUE NO SE VEA EL GRID, UTILIZAR Ctrl + "-" o Cmd + "-". (en terminal o VSC o donde sea ejecutado, si no se puede, usar ejemplos mas chicos...).