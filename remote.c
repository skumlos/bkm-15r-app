#include <stdio.h>
#include <sys/socket.h> 
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>

#define MONITOR_PORT (53484)
#define MONITOR_DEFAULT_IP "192.168.0.1"
#define ASPECT_STATUS (0x2)

int main(int argc , char *argv[])
{
	int sockfd;
    int rc = 0;
	struct sockaddr_in monitor;
    int disconnect = 0;
    struct termios ctrl;
    char command;
    const char header [6] = { 0x03, 0x0B, 'S', 'O', 'N', 'Y' };

    const char get_status[31] = { 0x03,0x0b,0x53,0x4f,0x4e,0x59,0x00,0x00,0x00,0xb0,0x00,0x00,0x12,0x53,0x54,0x41,0x54,0x67,0x65,0x74,0x20,0x43,0x55,0x52,0x52,0x45,0x4e,0x54,0x20,0x35,0x00 };
    const char menu_press[29] = { 0x03,0x0b,0x53,0x4f,0x4e,0x59,0x00,0x00,0x00,0xb0,0x00,0x00,0x10,0x49,0x4e,0x46,0x4f,0x62,0x75,0x74,0x74,0x6f,0x6e,0x20,0x4d,0x45,0x4e,0x55,0x20 };
    const char menu_up [31] =   { 0x03,0x0b,0x53,0x4f,0x4e,0x59,0x00,0x00,0x00,0xb0,0x00,0x00,0x12,0x49,0x4e,0x46,0x4f,0x62,0x75,0x74,0x74,0x6f,0x6e,0x20,0x4d,0x45,0x4e,0x55,0x55,0x50,0x20 };
    const char menu_down[33] =  { 0x03,0x0b,0x53,0x4f,0x4e,0x59,0x00,0x00,0x00,0xb0,0x00,0x00,0x14,0x49,0x4e,0x46,0x4f,0x62,0x75,0x74,0x74,0x6f,0x6e,0x20,0x4d,0x45,0x4e,0x55,0x44,0x4f,0x57,0x4e,0x20 };
    const char menu_enter[32] = { 0x03,0x0b,0x53,0x4f,0x4e,0x59,0x00,0x00,0x00,0xb0,0x00,0x00,0x13,0x49,0x4e,0x46,0x4f,0x62,0x75,0x74,0x74,0x6f,0x6e,0x20,0x4d,0x45,0x4e,0x55,0x45,0x4e,0x54,0x20 };
    const char status_power_toggle[33]  = { 0x03,0x0b,0x53,0x4f,0x4e,0x59,0x00,0x00,0x00,0xb0,0x00,0x00,0x14,0x53,0x54,0x41,0x54,0x73,0x65,0x74,0x20,0x50,0x4f,0x57,0x45,0x52,0x20,0x54,0x4f,0x47,0x47,0x4c,0x45 };
    const char status_aspect_toggle[34] = { 0x03,0x0b,0x53,0x4f,0x4e,0x59,0x00,0x00,0x00,0xb0,0x00,0x00,0x15,0x53,0x54,0x41,0x54,0x73,0x65,0x74,0x20,0x41,0x53,0x50,0x45,0x43,0x54,0x20,0x54,0x4f,0x47,0x47,0x4c,0x45 };
    char status_response[53];
    char button_response[13];

    fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
    tcgetattr(STDIN_FILENO, &ctrl);
    ctrl.c_lflag &= ~ICANON; // make input unbuffered, so enter after keypress isn't needed
    ctrl.c_lflag &= ~ECHO; // turn off echo so we don't see the 
    tcsetattr(STDIN_FILENO, TCSANOW, &ctrl);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1)
	{
		fprintf(stderr,"Could not create socket");
		return 1;
	}

    monitor.sin_addr.s_addr = inet_addr(MONITOR_DEFAULT_IP);
	monitor.sin_family = AF_INET;
	monitor.sin_port = htons(MONITOR_PORT);

    fprintf(stdout,"Connecting to monitor @ %s:%d\n",MONITOR_DEFAULT_IP,MONITOR_PORT);
	if (connect(sockfd , (struct sockaddr *)&monitor , sizeof(monitor)) < 0)
	{
		fprintf(stderr,"Could not connect");
		return 2;
    }

    fprintf(stdout,"Connected, starting loop\n");
    fprintf(stdout,"Supported keys:\n");
    fprintf(stdout,"p - Power toggle\n");
    fprintf(stdout,"a - Aspect ratio (16:9) toggle\n");
    fprintf(stdout,"m - Menu\n");
    fprintf(stdout,"Enter - Enter (menu)\n");   
    fprintf(stdout,"Arrow-up - Navigate up (menu)\n");   
    fprintf(stdout,"Arrow-down - Navigate down (menu)\n");   

    while(!disconnect) {
        if(read(0, &command, 1) == 1) {
            switch(command) {
                case 'm':
                    fprintf(stdout,"MENU\n");
                    if(send(sockfd, menu_press, sizeof(menu_press), 0) < 0) {
                        fprintf(stderr,"Failed sending MENU\n");
                        goto fail;
                    }
                    recv(sockfd, button_response,sizeof(button_response),0);
                break;
                case 0x0A:
                    fprintf(stdout,"MENU ENTER\n");
                    if(send(sockfd, menu_enter, sizeof(menu_enter), 0) < 0) {
                        fprintf(stderr,"Failed sending MENU ENTER\n");
                        goto fail;
                    }
                    recv(sockfd, button_response,sizeof(button_response),0);
                break;
                case 'a':
                    fprintf(stdout,"ASPECT\n");
                    if(send(sockfd, status_aspect_toggle, sizeof(status_aspect_toggle), 0) < 0) {
                        fprintf(stderr,"Failed sending ASPECT\n");
                        goto fail;
                    }
                    recv(sockfd, button_response,sizeof(button_response),0);
                break;
                case 'p':
                    fprintf(stdout,"POWER\n");
                    if(send(sockfd, status_power_toggle, sizeof(status_power_toggle), 0) < 0) {
                        fprintf(stderr,"Failed sending POWER\n");
                        goto fail;
                    }
                    recv(sockfd, button_response,sizeof(button_response),0);
                break;
                case 'q':
                    disconnect = 1;
                break;
                case 0x1B:
                    if(read(0, &command, 1) == 1) {
                        switch(command) {
                            case 0x5B:
                                if(read(0, &command, 1) == 1) {
                                    switch(command) {
                                        case 0x41: // up
                                            fprintf(stdout,"UP\n");
                                            if(send(sockfd, menu_up, sizeof(menu_up), 0) < 0) {
                                                fprintf(stderr,"Failed sending MENU UP\n");
                                                goto fail;
                                            }
                                            recv(sockfd, button_response,sizeof(button_response),0);
                                        break;
                                        case 0x42: // down
                                            fprintf(stdout,"DOWN\n");
                                            if(send(sockfd, menu_down, sizeof(menu_down), 0) < 0) {
                                                fprintf(stderr,"Failed sending MENU DOWN\n");
                                                goto fail;
                                            }
                                            recv(sockfd, button_response,sizeof(button_response),0);
                                        break;
                                        default:
                                        break;
                                    }
                                }
                            break;
                            default:
                            break;
                        }
                    }
                break;
                default:
                break;
            }
        }

        if(send(sockfd, get_status, sizeof(get_status), 0) < 0)
	    {
		    fprintf(stderr,"Sending get status failed...\n");
		    goto fail;
	    }
        recv(sockfd,status_response,sizeof(status_response),0);
        usleep(500*1000);
    }
    goto close;

fail:
    rc = 3;

close:
    fprintf(stderr,"Closing connection, and exiting...\n");
    close(sockfd);
    ctrl.c_lflag |= ECHO; // turn echo back on again
    ctrl.c_lflag |= ICANON; // make input buffered again
    tcsetattr(STDIN_FILENO, TCSANOW, &ctrl);
    return rc;
}