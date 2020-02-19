icmp-reset

Esta herramienta de auditoria fue generada con el fin de ayudar a fabricantes
de equipos y administradores de redes a auditar sus sistemas. Permite
configurar muchos parametros de la auditoria. Por defecto, muchos de ellos
tendran valores aleatorios. De este modo, la herramienta podra ser util
tambien para afinar la configuracion de sistemas de deteccion de intrusos
(IDSs), etc.

Cualquier tipo de comentario es bienvenido. Me puedes cantactar en
<fernando@gont.com.ar>.

Podras encontrar una descripcion del ataque "blind connection-reset" y de
las posibles contramedidas en el Internet-Draft que he publicado en la IETF.
Puedes conseguir el mismo en:

        http://www.gont.com.ar/drafts/icmp-attacks-against-tcp.html


A aquellos fabricantes que todavia no hayan contactado a NISCC (National
Infrastrucure Security Co-ordination Centre), se les sugiere hacerlo.
Pueden contactar a NISCC en su direccion de e-mail: <vulteam@niscc.gov.uk>


Un breve explicacion de la herramienta

Uso tipico:

icmp-reset -c 192.168.0.1:1024-1100 -s 172.16.0.1:80 -t client -r 10

La opcion "-c" especifica los datos del cliente. Es obligatorio especificar
la direccion IP del cliente. El numero de puerto (o rango de puertos) del
cliente es opcional. Si no especificas ningun puerto, la herramienta probara
todos los puertos del rango 0-65535. Si especificas un rango, la herramienta
probara solo ese rango. Si especificas un unico puerto, por ejemplo
"-c 192.168.0.1:1024", la herramienta probara solo ese puerto (y como
consecuencia el ataque requerira menos paquetes).

La opcion "-s" permite especificar los datos del servidor. Se aplican las
mismas consideraciones que para la opcion "-c". Ten presente que si no
especificas ningun puerto (ni para el cliente, ni para el servidor),
entonces se requeriran 65536*65536 paquetes para realizar el ataque.

La opcion "-t" especifica quien sera el blanco de los paquetes. En este caso,
los paquetes seran enviados al cliente.

La opcion "-r" permite limitar el ancho de banda a ser utilizado por el
ataque (en kbps). En este caso, estariamos limitando el ancho de banda
utilizando para el ataque en 10 kbps (obviamente, se trata de una
estimacion). Esta opcion es util en caso que algun router intermediario
este limitando el ancho de banda utilizado por el protocolo ICMP, y como
consecuencia pueda descartar aquellos paquetes que superen un nivel
predeterminado. Tambien puede ser util en caso que alguno de los sistemas
intervinientes no pueda dar a basto para procesar los paquetes generados
por la herramienta,  y como consecuencia descarte algunos de ellos.

Por defecto, se utilizara como direccion de origen de los paquetes aquella
del host que no esta siendo atacado. En este caso, la direccion IP de
origen seria "172.16.0.1". La opcion "-n" hara que los paquetes se envien
utilizando como direccion de origen tu propia direccion IP (es decir, la
direccion IP del "atacante"). Esto puede ser util si algun sistema
intermediario esta realizando "egress-filtering" (filtro de egreso).

Adicionalmente, la opcion "-f" permite especificar la direccion de origen
de los paquetes. Por ejemplo, el comando:

icmp-reset -c 192.168.0.1:1024-1100 -s 172.16.0.1:80 -t client -r 10 -f 10.0.0.1

hara que se utilice como direccion IP de origen de los paquetes ICMP la
direccion "10.0.0.1".

Por defecto, muchos campos utilizaran valores aleatorios (el TTL, el TTL del
payload, el IP ID, el IP ID del payload, el numero de secuencia TCP, etc.).
Tanto el TOS del paquete generado como el del encabezamiento IP contenido
en el cuerpo del mensaje ICMP utilizaran el valor 0x00 (trafico normal).

Por defecto, los paquetes ICMP enviados seran de tipo 3 ("Destino
Inalcanzable"), codigo 2 ("Protocolo Inalcanzable"), pero puedes seleccionar
cualquier codigo ICMP mediante la opcion "-i".

Por defecto, el campo "total length" (largo total) del encabezamiento IP
contenido en el cuerpo del mensaje ICMP tendra el valor 576. Sin emabargo,
puedes especificar cualquier tama¤o por medio de la opcion "-z". Nota que
esto no afecta la cantidad de bytes que seran transmitidos. Esta opcion
simplemente hara que se coloque en el campo "total length" el valor
especificado

--
Fernando Gont
e-mail: fernando@gont.com.ar


