/****************************************************************************/
/* Plantilla para implementación de funciones del cliente (rcftpclient)     */
/* $Revision$ */
/* Aunque se permite la modificación de cualquier parte del código, se */
/* recomienda modificar solamente este fichero y su fichero de cabeceras asociado. */
/****************************************************************************/

/**************************************************************************/
/* INCLUDES                                                               */
/**************************************************************************/
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include "rcftp.h" // Protocolo RCFTP
#include "rcftpclient.h" // Funciones ya implementadas
#include "multialarm.h" // Gestión de timeouts
#include "vemision.h" // Gestión de ventana de emisión
#include "misfunciones.h"

/**************************************************************************/
/* VARIABLES GLOBALES                                                     */
/**************************************************************************/
#warning HAY QUE PONER EL NOMBRE (Y BORRAR EL WARNING)
// elegir 1 o 2 autores y sustituir "Apellidos, Nombre" manteniendo el formato
char* autores="Autor: Apellidos, Nombre"; // un solo autor
//char* autores="Autor: Apellidos, Nombre\nAutor: Apellidos, Nombre" // dos autores

// variable para indicar si mostrar información extra durante la ejecución
// como la mayoría de las funciones necesitaran consultarla, la definimos global
extern char verb;


// variable externa que muestra el número de timeouts vencidos
// Uso: Comparar con otra variable inicializada a 0; si son distintas, tratar un timeout e incrementar en uno la otra variable
extern volatile const int timeouts_vencidos;


/**************************************************************************/
/* Obtiene la estructura de direcciones del servidor */
/**************************************************************************/
struct addrinfo* obtener_struct_direccion(char *dir_servidor, char *servicio, char f_verbose){
	struct addrinfo hints,     // Variable para especificar la solicitud
                    *servinfo, // Puntero para respuesta de getaddrinfo()
                    *direccion;// Puntero para recorrer la lista de
                               // direcciones de servinfo
    int status;     // Finalización correcta o no de la llamada getaddrinfo()
    int numdir = 1; // Contador de estructuras de direcciones en la
                    // lista de direcciones de servinfo

    // sobreescribimos con ceros la estructura
    // para borrar cualquier dato que pueda malinterpretarse
    memset(&hints, 0, sizeof hints);

    // genera una estructura de dirección con especificaciones de la solicitud
    if (f_verbose)
    {
        printf("1 - Especificando detalles de la estructura de direcciones a solicitar... \n");
        fflush(stdout);
    }

    // especificamos la familia de direcciones con la que queremos trabajar:
    // AF_UNSPEC, AF_INET (IPv4), AF_INET6 (IPv6), etc.
    hints.ai_family = AF_UNSPEC;

    if (f_verbose)
    {
        printf("\tFamilia de direcciones/protocolos: ");
        switch (hints.ai_family)
        {
            case AF_UNSPEC: printf("IPv4 e IPv6\n"); break;
            case AF_INET:   printf("IPv4\n"); break;
            case AF_INET6:  printf("IPv6\n"); break;
            default:        printf("No IP (%d)\n", hints.ai_family); break;
        }
        fflush(stdout);
    }

    // especificamos el tipo de socket deseado:
    // SOCK_STREAM (TCP), SOCK_DGRAM (UDP), etc.
    hints.ai_socktype = SOCK_DGRAM;

    if (f_verbose)
    {
        printf("\tTipo de comunicación: ");
        switch (hints.ai_socktype)
        {
            case SOCK_STREAM: printf("flujo (TCP)\n"); break;
            case SOCK_DGRAM:  printf("datagrama (UDP)\n"); break;
            default:          printf("no convencional (%d)\n", hints.ai_socktype); break;
        }
        fflush(stdout);
    }

    // flags específicos dependiendo de si queremos la dirección como cliente
    // o como servidor
    if (dir_servidor != NULL)
    {
        // si hemos especificado dir_servidor, es que somos el cliente
        // y vamos a conectarnos con dir_servidor
        if (f_verbose) printf("\tNombre/dirección del equipo: %s\n", dir_servidor);
    }
    else
    {
        // si no hemos especificado, es que vamos a ser el servidor
        if (f_verbose) printf("\tNombre/dirección: equipo local\n");

        // especificar flag para que la IP se rellene con lo necesario para hacer bind
        // consultar documentación con: 'man getaddrinfo')
        hints.ai_flags = AI_PASSIVE;
    }
    if (f_verbose) printf("\tServicio/puerto: %s\n", servicio);

    // llamada getaddrinfo() para obtener la estructura de direcciones solicitada
    // getaddrinfo() pide memoria dinámica al SO,
    // la rellena con la estructura de direcciones
    // y escribe en servinfo la dirección donde se encuentra dicha estructura.
    // La memoria dinámica reservada en una función NO se libera al salir de ella
    // Para liberar esta memoria, usar freeaddrinfo()
    if (f_verbose)
    {
        printf("2 - Solicitando la estructura de direcciones con getaddrinfo()... ");
        fflush(stdout);
    }
    status = getaddrinfo(dir_servidor, servicio, &hints, &servinfo);
    if (status != 0)
    {
        fprintf(stderr,"Error en la llamada getaddrinfo(): %s\n", gai_strerror(status));
        exit(1);
    }
    if (f_verbose) printf("hecho\n");

    // imprime la estructura de direcciones devuelta por getaddrinfo()
    if (f_verbose)
    {
        printf("3 - Analizando estructura de direcciones devuelta... \n");
        direccion = servinfo;
        while (direccion != NULL)
        {   // bucle que recorre la lista de direcciones
            printf("    Dirección %d:\n", numdir);
            printsockaddr((struct sockaddr_storage*) direccion->ai_addr);
            // "avanzamos" a la siguiente estructura de direccion
            direccion = direccion->ai_next;
            numdir++;
        }
    }

    // devuelve la estructura de direcciones devuelta por getaddrinfo()
    return servinfo;
}

/**************************************************************************/
/* Imprime una direccion */
/**************************************************************************/
void printsockaddr(struct sockaddr_storage * saddr) {
	struct sockaddr_in *saddr_ipv4; // puntero a estructura de dirección IPv4
    // el compilador interpretará lo apuntado como estructura de dirección IPv4
    struct sockaddr_in6 *saddr_ipv6; // puntero a estructura de dirección IPv6
    // el compilador interpretará lo apuntado como estructura de dirección IPv6
    void *addr; // puntero a dirección
    // como puede ser tipo IPv4 o IPv6 no queremos que el compilador la
    // interprete de alguna forma particular, por eso void
    char ipstr[INET6_ADDRSTRLEN]; // string para la dirección en formato texto
    int port; // almacena el número de puerto al analizar estructura devuelta

    if (saddr == NULL)
    {
        printf("La dirección está vacía\n");
    }
    else
    {
        printf("\tFamilia de direcciones: ");
        fflush(stdout);
        if (saddr->ss_family == AF_INET6)
        {   // IPv6
            printf("IPv6\n");
            // apuntamos a la estructura con saddr_ipv6 (cast evita warning),
            // así podemos acceder al resto de campos a través de
            // este puntero sin más casts
            saddr_ipv6 = (struct sockaddr_in6 *)saddr;
            // apuntamos al campo de la estructura que contiene la dirección
            addr = &(saddr_ipv6->sin6_addr);
            // obtenemos el puerto, pasando del formato de red al formato local
            port = ntohs(saddr_ipv6->sin6_port);
        }
        else if (saddr->ss_family == AF_INET)
        {   // IPv4
            printf("IPv4\n");
            saddr_ipv4 = (struct sockaddr_in *)saddr;            
            addr = &(saddr_ipv4->sin_addr); 
            port = ntohs(saddr_ipv4->sin_port); 
        }
        else
        {
            fprintf(stderr, "familia desconocida\n");
            exit(1);
        }
        // convierte la dirección ip a string
        inet_ntop(saddr->ss_family, addr, ipstr, sizeof ipstr);
        printf("\tDirección (interpretada según familia): %s\n", ipstr);
        printf("\tPuerto (formato local): %d\n", port);
    }
}

/**************************************************************************/
/* Configura el socket, devuelve el socket y servinfo */
/**************************************************************************/
int initsocket(struct addrinfo *servinfo, char f_verbose){
	int sock = -1;
    int numdir = 1;

    while (servinfo != NULL && sock < 0)
    {   // bucle que recorre la lista de direcciones
        printf("Intentando conexión con dirección %d:\n", numdir);

        // crea un extremo de la comunicación y devuelve un descriptor
        if (f_verbose)
        {
            printf("Creando el socket (socket)... ");
            fflush(stdout);
        }
        sock = socket(servinfo->ai_family,servinfo->ai_socktype,servinfo->ai_protocol);
        if (sock < 0)
        {
            perror("Error en la llamada socket: No se pudo crear el socket");
            // muestra por pantalla el valor de la cadena suministrada por el
            // programador, dos puntos y un mensaje de error que detalla la
            // causa del error cometido
        }
        

        // "avanzamos" a la siguiente estructura de direccion
        servinfo = servinfo->ai_next;
        numdir++;
    }

    if (sock < 0)
    {
        perror("No se ha podido establecer la comunicación");
        exit(1);
    }

    return sock;
}



/**************************************************************************/
/* Envía un mensaje a la dirección especificada */
/**************************************************************************/
void enviarmensaje(int s, struct rcftp_msg sendbuffer, struct sockaddr * remote, socklen_t remotelen, unsigned int flags) {
	ssize_t sentsize;

	if ((sentsize=sendto(s,(char *)&sendbuffer,sizeof(sendbuffer),0,remote,remotelen)) != sizeof(sendbuffer)) {
		if (sentsize!=-1)
			fprintf(stderr,"Error: enviados %d bytes de un mensaje de %d bytes\n",(int)sentsize,(int)sizeof(sendbuffer));
		else
			perror("Error en sendto");
		//exit(S_SYSERROR);
	} 

	// print response if in verbose mode
	/*if (flags & F_VERBOSE) {
		printf("Mensaje RCFTP " ANSI_COLOR_MAGENTA "enviado" ANSI_COLOR_RESET ":\n");
		print_rcftp_msg(&sendbuffer,sizeof(sendbuffer));
	} */
}	

ssize_t recibirmensaje(int socket, struct rcftp_msg *buffer, int buflen, struct sockaddr_storage *remote, socklen_t *remotelen) {
	ssize_t recvsize;
	
	*remotelen = sizeof(*remote);
	recvsize = recvfrom(socket,(char *)buffer,buflen,0,(struct sockaddr *)remote,remotelen);
	if (recvsize<0 && errno!=EAGAIN) { // en caso de socket no bloqueante
		//if (recvsize<0 && errno!=EINTR) { // en caso de socket bloqueante (no funciona en GNU/Linux)
		perror("Error en recvfrom: ");
		// exit(S_SYSERROR);
	} else if (*remotelen>sizeof(*remote)) {
		fprintf(stderr,"Error: la dirección del cliente ha sido truncada\n");
		// exit(S_SYSERROR);
	}
	return recvsize;
}

/**************************************************************************/
/* Verifica version,next,checksum */
/**************************************************************************/
int esMensajeValido(struct rcftp_msg recvbuffer) { 
	int esperado=1;
	//uint16_t aux;

	if (recvbuffer.version!=RCFTP_VERSION_1) { // versión incorrecta
		esperado=0;
		fprintf(stderr,"Error: recibido un mensaje con versión incorrecta\n");
	}
	// if (recvbuffer.next!=0) { // next incorrecto
	// 	esperado=0;
	// 	fprintf(stderr,"Error: recibido un mensaje con NEXT incorrecto\n");
	// }
	if (issumvalid(&recvbuffer,sizeof(recvbuffer))==0) { // checksum incorrecto
		esperado=0;
		fprintf(stderr,"Error: recibido un mensaje con checksum incorrecto\n"); /* (esperaba ");
		aux=recvbuffer.sum;
		recvbuffer.sum=0;
		fprintf(stderr,"0x%x)\n",ntohs(xsum((char*)&recvbuffer,sizeof(recvbuffer))));
		recvbuffer.sum=aux;*/
	}
	return esperado;
}

int esLaRespuestaEsperada(struct rcftp_msg recvbuffer, struct rcftp_msg mensaje){
    int esperado=1;
    if(ntohl(recvbuffer.next) != (ntohl(mensaje.numseq) + ntohs(mensaje.len))){
        esperado=0;

        //printf("recvbuffer.next: %d\n", ntohl(recvbuffer.next));
        //printf("mensaje.numseq: %d\n", ntohl(mensaje.numseq));
        //printf("mensaje.len: %d\n", ntohs(mensaje.len));

		fprintf(stderr,"Error: recvbuffer.next != mensaje.numseq + mensaje.len\n");
    }
    if(recvbuffer.flags == F_ABORT){
        esperado=0;
		fprintf(stderr,"Error: F_ABORT\n");
    }
    if(recvbuffer.flags == F_BUSY){
        esperado=0;
		fprintf(stderr,"Error: F_BUSY\n");
    }
    if(mensaje.flags == F_FIN && recvbuffer.flags != F_FIN){
        esperado=0;
		fprintf(stderr,"Error: mensaje.flags == F_FIN && recvbuffer.flags != F_FIN\n");
    }
    return esperado;
}

struct rcftp_msg construirMensajeRCFTP(char* datos, ssize_t recvsize, uint8_t ultimoMensaje, int* seq){
	struct rcftp_msg sendbuffer;
	//uint32_t next_calculado;
    

	// calcular next ***********************************************
	// empezar sin flags activos
    if(ultimoMensaje == 1){
        sendbuffer.flags=F_FIN;
    }
    else{
        sendbuffer.flags=F_NOFLAGS;
    }

	// construir el mensaje válido ***********************************
	// los flags los hemos ido rellenando al calcular el next
	sendbuffer.version=RCFTP_VERSION_1;
	sendbuffer.numseq=htonl(*seq);
	// sendbuffer.len=htons(adddata()); // nunca respondemos con datos
	sendbuffer.len=htons(recvsize);
	for(int i = 0; i < recvsize; i++){
		sendbuffer.buffer[i]=datos[i];
	}
	sendbuffer.next=htonl(0);
	sendbuffer.sum=0;
	sendbuffer.sum=xsum((char*)&sendbuffer,sizeof(sendbuffer));

    *seq = *seq + recvsize;

	return sendbuffer;
}

/**************************************************************************/
/*  algoritmo 1 (basico)  */
/**************************************************************************/
void alg_basico(int socket, struct addrinfo *servinfo) {
	uint8_t ultimoMensaje, ultimoMensajeConfirmado;
	char	datos[RCFTP_BUFLEN];
	ssize_t recvsize;
	struct rcftp_msg mensaje;
	int seq;
    struct rcftp_msg recvbuffer;
	struct sockaddr_storage	remote;
	socklen_t remotelen;

	printf("Comunicación con algoritmo básico\n");
	ultimoMensaje = 0;
	ultimoMensajeConfirmado = 0;

	recvsize = readtobuffer(datos, RCFTP_BUFLEN);

	if(recvsize == 0){
		ultimoMensaje = 1;
	}

	seq = 0;
	mensaje = construirMensajeRCFTP(datos, recvsize, ultimoMensaje, &seq);

	while(ultimoMensajeConfirmado == 0){
		printf("Enviando mensaje\n");
		enviarmensaje(socket ,mensaje, servinfo->ai_addr, sizeof *servinfo, 0);
		print_rcftp_msg(&mensaje, sizeof mensaje);
		recibirmensaje(socket,&mensaje,sizeof(mensaje),&remote,&remotelen);

        if(esMensajeValido(recvbuffer) && esLaRespuestaEsperada(recvbuffer, mensaje)){
            if(ultimoMensaje){
                ultimoMensajeConfirmado = 1;
            }
            else{
                recvsize = readtobuffer(datos, RCFTP_BUFLEN);
                if(recvsize == 0){
                    ultimoMensaje = 1;
                }
                mensaje = construirMensajeRCFTP(datos, recvsize, ultimoMensaje, &seq);
            }
		}
	}
}

/**************************************************************************/
/*  algoritmo 2 (stop & wait)  */
/**************************************************************************/
void alg_stopwait(int socket, struct addrinfo *servinfo) {

	printf("Comunicación con algoritmo stop&wait\n");

#warning FALTA IMPLEMENTAR EL ALGORITMO STOP-WAIT
	printf("Algoritmo no implementado\n");
}

/**************************************************************************/
/*  algoritmo 3 (ventana deslizante)  */
/**************************************************************************/
void alg_ventana(int socket, struct addrinfo *servinfo,int window) {

	printf("Comunicación con algoritmo go-back-n\n");

#warning FALTA IMPLEMENTAR EL ALGORITMO GO-BACK-N
	printf("Algoritmo no implementado\n");
}


