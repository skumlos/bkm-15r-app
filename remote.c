// Crude BKM-15R emulator for Linux terminal
// (2022) Martin Hejnfelt (martin@hejnfelt.com)
// See https://immerhax.com/?p=797 for more information
// Licensed under the WTFPL, see LICENSE.txt

#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <stdint.h>
#include <string.h>

#define MONITOR_PORT (53484)
#define MONITOR_DEFAULT_IP "192.168.0.1"

// status word 1
#define POWER_ON_STATUS     (0x8000)
#define SCANMODE_STATUS     (0x0400)
#define HDELAY_STATUS       (0x0200)
#define VDELAY_STATUS       (0x0100)
#define MONOCHROME_STATUS   (0x0080)
#define CHAR_MUTE_STATUS    (0x0040)
#define MARKER_MODE_STATUS  (0x0020)
#define EXTSYNC_STATUS      (0x0010)
#define APT_STATUS          (0x0008)
#define CHROMA_UP_STATUS    (0x0004)
#define ASPECT_STATUS       (0x0002)

// status word 2
// Unknown / unused

// status word 3
#define COL_TEMP_STATUS     (0x0040)
#define COMB_STATUS         (0x0020)
#define BLUE_ONLY_STATUS    (0x0010)
#define R_CUTOFF_STATUS     (0x0004)
#define G_CUTOFF_STATUS     (0x0002)
#define B_CUTOFF_STATUS     (0x0001)

// status word 4
#define MAN_PHASE_STATUS    (0x0080)
#define MAN_CHROMA_STATUS   (0x0040)
#define MAN_BRIGHT_STATUS   (0x0020)
#define MAN_CONTRAST_STATUS (0x0010)

// status word 5
// Unknown / unused

// Status buttons
#define POWER_BUTTON        "POWER"
#define DEGAUSS_BUTTON      "DEGAUSS"

#define SCANMODE_BUTTON     "SCANMODE"
#define HDELAY_BUTTON       "HDELAY"
#define VDELAY_BUTTON       "VDELAY"
#define MONOCHROME_BUTTON   "MONOCHR"
#define APERTURE_BUTTON     "APERTURE"
#define COMB_BUTTON         "COMB"
#define CHAR_OFF_BUTTON     "CHARMUTE"
#define COL_TEMP_BUTTON     "COLADJ"

#define ASPECT_BUTTON       "ASPECT"
#define EXTSYNC_BUTTON      "EXTSYNC"
#define BLUE_ONLY_BUTTON    "BLUEONLY"
#define R_CUTOFF_BUTTON     "RCUTOFF"
#define G_CUTOFF_BUTTON     "GCUTOFF"
#define B_CUTOFF_BUTTON     "BCUTOFF"
#define MARKER_BUTTON       "MARKER"
#define CHROMA_UP_BUTTON    "CHROMAUP"

#define MAN_PHASE_BUTTON    "MANPHASE"
#define MAN_CHROMA_BUTTON   "MANCHR"
#define MAN_BRIGHT_BUTTON   "MANBRT"
#define MAN_CONTRAST_BUTTON "MANCONT"

#define TOGGLE              "TOGGLE"
#define CURRENT             "CURRENT"
#define STATUS_GET          "STATget"
#define STATUS_SET          "STATset"

// Info buttons/knobs
#define INFO_INP_ENTER      "ENTER"
#define INFO_INP_DELETE     "DELETE"
#define INFO_NAV_MENU       "MENU"
#define INFO_NAV_MENUENT    "MENUENT"
#define INFO_NAV_MENUUP     "MENUUP"
#define INFO_NAV_MENUDOWN   "MENUDOWN"

#define INFO_BUTTON         "INFObutton"

#define INFO_KNOB_PHASE         "R PHASE"
#define INFO_KNOB_CHROMA        "R CHROMA"
#define INFO_KNOB_BRIGHTNESS    "R BRIGHTNESS"
#define INFO_KNOB_CONTRAST      "R CONTRAST"

#define INFO_KNOB           "INFOknob"

const char header [13] = { 0x03, 0x0B, 'S', 'O', 'N', 'Y', 0x00, 0x00, 0x00, 0xB0, 0x00, 0x00, 0x00 };
uint8_t status_response[53];
uint8_t button_response[13];
uint8_t packetBuf[40];
int sockfd;

uint8_t sendInfoButtonPacket(const char* button) {
    uint8_t dataLength = sizeof(header)-1;
    packetBuf[dataLength++] = strlen(INFO_BUTTON) + strlen(button) + 2;
    memcpy(packetBuf+dataLength, INFO_BUTTON, strlen(INFO_BUTTON));
    dataLength += strlen(INFO_BUTTON);
    packetBuf[dataLength++] = 0x20;
    memcpy(packetBuf+dataLength, button, strlen(button));
    dataLength += strlen(button);
    packetBuf[dataLength++] = 0x20;
    if(send(sockfd, packetBuf, dataLength, 0) < 0) {
        fprintf(stderr,"Failed sending %s\n",button);
        return 1;
    }
    recv(sockfd, button_response,sizeof(button_response),0);
    return 0;
}

uint8_t sendStatusButtonTogglePacket(const char* button) {
    uint8_t dataLength = sizeof(header)-1;
    packetBuf[dataLength++] = strlen(STATUS_SET) + strlen(button) + strlen(TOGGLE) + 2;
    memcpy(packetBuf+dataLength, STATUS_SET, strlen(STATUS_SET));
    dataLength += strlen(STATUS_SET);
    packetBuf[dataLength++] = 0x20;
    memcpy(packetBuf+dataLength, button, strlen(button));
    dataLength += strlen(button);
    packetBuf[dataLength++] = 0x20;
    memcpy(packetBuf+dataLength, TOGGLE, strlen(TOGGLE));
    dataLength += strlen(TOGGLE);
    if(send(sockfd, packetBuf, dataLength, 0) < 0) {
        fprintf(stderr,"Failed sending %s\n",button);
        return 1;
    }
    recv(sockfd, button_response,sizeof(button_response),0);
    return 0;
}

enum Knobs {
    KNOB_PHASE,
    KNOB_CHROMA,
    KNOB_BRIGHT,
    KNOB_CONTRAST,
    KNOB_NONE
};

int currentKnob = KNOB_NONE;
char knobStatus[20];

void turnKnob(int8_t dir, uint8_t ticks) {
    uint8_t dataLength = sizeof(header)-1;
    char *knob = NULL;
    if(currentKnob == KNOB_NONE) return;
 
    snprintf(knobStatus,20,"96/%d/%d",dir,ticks);

    switch(currentKnob) {
        case KNOB_PHASE:
            knob = INFO_KNOB_PHASE;
        break;
        case KNOB_CHROMA:
            knob = INFO_KNOB_CHROMA;
        break;
        case KNOB_BRIGHT:
            knob = INFO_KNOB_BRIGHTNESS;
        break;
        case KNOB_CONTRAST:
        default:
            knob = INFO_KNOB_CONTRAST;
        break;
    }
    packetBuf[dataLength++] = strlen(INFO_KNOB) + strlen(knob) + strlen(knobStatus) + 2;
    memcpy(packetBuf+dataLength, INFO_KNOB, strlen(INFO_KNOB));
    dataLength += strlen(INFO_KNOB);
    packetBuf[dataLength++] = 0x20;
    memcpy(packetBuf+dataLength, knob, strlen(knob));
    dataLength += strlen(knob);
    packetBuf[dataLength++] = 0x20;
    memcpy(packetBuf+dataLength, knobStatus, strlen(knobStatus));
    dataLength += strlen(knobStatus);
    if(send(sockfd, packetBuf, dataLength, 0) < 0) {
        fprintf(stderr,"Failed sending %s\n",knob);
        return;
    }
    recv(sockfd, button_response,sizeof(button_response),0);
}

int main(int argc , char *argv[])
{
    int rc = 0;
	struct sockaddr_in monitor;
    int disconnect = 0;
    struct termios ctrl;
    uint8_t dataLength,l,knobselect=0,knobchanged=1;
    uint16_t statusw1 = 0xFFFF,_statusw1 = 0xFFFF;
    uint16_t statusw2 = 0xFFFF,_statusw2 = 0xFFFF;
    uint16_t statusw3 = 0xFFFF,_statusw3 = 0xFFFF;
    uint16_t statusw4 = 0xFFFF,_statusw4 = 0xFFFF;
    uint16_t statusw5 = 0xFFFF,_statusw5 = 0xFFFF;
    char command;

    const char get_status[31] = { 0x03,0x0b,0x53,0x4f,0x4e,0x59,0x00,0x00,0x00,0xb0,0x00,0x00,0x12,0x53,0x54,0x41,0x54,0x67,0x65,0x74,0x20,0x43,0x55,0x52,0x52,0x45,0x4e,0x54,0x20,0x35,0x00 };

    printf("Sony BKM-15R emulator\n");
    printf("(2022) Martin Hejnfelt (martin@hejnfelt.com)\n");
    printf("www.immerhax.com\n\n");

    memcpy(packetBuf,header,sizeof(header));
 
    fcntl(0, F_SETFL, fcntl(0, F_GETFL) | O_NONBLOCK);
    tcgetattr(STDIN_FILENO, &ctrl);
    ctrl.c_lflag &= ~ICANON; // make input unbuffered, so enter after keypress isn't needed
    ctrl.c_lflag &= ~ECHO; // turn off echo so we don't see the keypresses
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
    fprintf(stdout,"P - (P)ower\n");
    fprintf(stdout,"D - (D)egauss\n");
    fprintf(stdout,"\n"); 
    fprintf(stdout,"u - (u)nderscan (scanmode)\n");
    fprintf(stdout,"h - (h)orizontal delay\n");
    fprintf(stdout,"h - (v)ertical delay\n");
    fprintf(stdout,"o - M(o)nochrome\n");
    fprintf(stdout,"A - (A)perture)\n");
    fprintf(stdout,"c - (c)omb\n");
    fprintf(stdout,"C - (C)har off (charmute)\n");
    fprintf(stdout,"T - Color (T)emp\n");
    fprintf(stdout,"\n");
    fprintf(stdout,"a - (a)spect ratio (16:9)\n");
    fprintf(stdout,"s - External (s)ync\n");
    fprintf(stdout,"B - (B)lue only\n");
    fprintf(stdout,"r - (r) cutoff\n");
    fprintf(stdout,"g - (g) cutoff\n");
    fprintf(stdout,"b - (b) cutoff\n");
    fprintf(stdout,"K - Mar(K)er\n");
    fprintf(stdout,"U - Chroma (U)p\n");
    fprintf(stdout,"\n"); 
    fprintf(stdout,"k - Select active knob\n");
    fprintf(stdout,"+/- - Turn current knob clockwise (+) or counterclockwise (-)\n");
    fprintf(stdout,"\n");
    fprintf(stdout,"H - Manual P(H)ase\n");
    fprintf(stdout,"R - Manual Ch(R)oma\n");
    fprintf(stdout,"I - Manual Br(I)ghtness\n");
    fprintf(stdout,"N - Manual Co(N)trast\n");
    fprintf(stdout,"\n");
    fprintf(stdout,"m - (m)enu\n");
    fprintf(stdout,"Enter - Enter (menu)\n");   
    fprintf(stdout,"Arrow-up - Navigate up (menu)\n");   
    fprintf(stdout,"Arrow-down - Navigate down (menu)\n");   
    fprintf(stdout,"0-9 - Input key 0-9\n");
    fprintf(stdout,"e - Input (e)nter\n");
    fprintf(stdout,"d - Input (d)elete\n");
    fprintf(stdout,"\nq - Quit program\n\n");
 
    while(!disconnect) {
        if(read(0, &command, 1) == 1) {
            if(knobselect) {
                switch(command) {
                    case 'H':
                        currentKnob = KNOB_PHASE;
                    break;
                    case 'R':
                        currentKnob = KNOB_CHROMA;
                    break;
                    case 'I':
                        currentKnob = KNOB_BRIGHT;
                    break;
                    case 'N':
                        currentKnob = KNOB_CONTRAST;
                    break;
                    default:
                        currentKnob = KNOB_NONE;
                    break;
                }
                knobselect = 0;
                knobchanged = 1;
            } else {
                switch(command) {
                    case '+':
                        turnKnob(1,1);
                    break;
                    case '-':
                        turnKnob(-1,1);
                    break;
                    case '0':
                    case '1':
                    case '2':
                    case '3':
                    case '4':
                    case '5':
                    case '6':
                    case '7':
                    case '8':
                    case '9':
                        dataLength = sizeof(header)-1;
                        packetBuf[dataLength++] = strlen(INFO_BUTTON) + 3;
                        memcpy(packetBuf+dataLength, INFO_BUTTON, strlen(INFO_BUTTON));
                        dataLength += strlen(INFO_BUTTON);
                        packetBuf[dataLength++] = 0x20;
                        packetBuf[dataLength++] = command;
                        packetBuf[dataLength++] = 0x20;
                        if(send(sockfd, packetBuf, dataLength, 0) < 0) {
                            goto fail;
                        }
                        recv(sockfd, button_response,sizeof(button_response),0);
                    break;
                    case 'd':
                        if(sendInfoButtonPacket(INFO_INP_DELETE)) goto fail;
                    break;
                    case 'e':
                        if(sendInfoButtonPacket(INFO_INP_ENTER)) goto fail;
                    break;                
                    case 'm':
                        if(sendInfoButtonPacket(INFO_NAV_MENU)) goto fail;
                    break;
                    case 0x1B:
                        if(read(0, &command, 1) == 1) {
                            switch(command) {
                                case 0x5B:
                                    if(read(0, &command, 1) == 1) {
                                        switch(command) {
                                            case 0x41: // up
                                                if(sendInfoButtonPacket(INFO_NAV_MENUUP)) goto fail;
                                            break;
                                            case 0x42: // down
                                                if(sendInfoButtonPacket(INFO_NAV_MENUDOWN)) goto fail;
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
                    case 0x0A:
                        if(sendInfoButtonPacket(INFO_NAV_MENUENT)) goto fail;
                    break;
                    case 'P':
                        if(sendStatusButtonTogglePacket(POWER_BUTTON)) goto fail;
                    break;
                    case 'D':
                        dataLength = sizeof(header)-1;
                        packetBuf[dataLength++] = strlen(STATUS_SET) + strlen(DEGAUSS_BUTTON) + strlen(TOGGLE) + 2;
                        memcpy(packetBuf+dataLength, STATUS_SET, strlen(STATUS_SET));
                        dataLength += strlen(STATUS_SET);
                        packetBuf[dataLength++] = 0x20;
                        memcpy(packetBuf+dataLength, DEGAUSS_BUTTON, strlen(DEGAUSS_BUTTON));
                        dataLength += strlen(DEGAUSS_BUTTON);
                        packetBuf[dataLength++] = 0x20;
                        if(send(sockfd, packetBuf, dataLength, 0) < 0) {
                            fprintf(stderr,"Failed sending %s\n",DEGAUSS_BUTTON);
                            goto fail;
                        }
                        recv(sockfd, button_response,sizeof(button_response),0);
                    break;
                    case 'u':
                        if(sendStatusButtonTogglePacket(SCANMODE_BUTTON)) goto fail;
                    break;
                    case 'h':
                        if(sendStatusButtonTogglePacket(HDELAY_BUTTON)) goto fail;
                    break;
                    case 'v':
                        if(sendStatusButtonTogglePacket(VDELAY_BUTTON)) goto fail;
                    break;
                    case 'o':
                        if(sendStatusButtonTogglePacket(MONOCHROME_BUTTON)) goto fail;
                    break;
                    case 'A':
                        if(sendStatusButtonTogglePacket(APERTURE_BUTTON)) goto fail;
                    break;
                    case 'c':
                        if(sendStatusButtonTogglePacket(COMB_BUTTON)) goto fail;
                    break;
                    case 'C':
                        if(sendStatusButtonTogglePacket(CHAR_OFF_BUTTON)) goto fail;
                    break;
                    case 'T':
                        if(sendStatusButtonTogglePacket(COL_TEMP_BUTTON)) goto fail;
                    break;
                    case 'a':
                        if(sendStatusButtonTogglePacket(ASPECT_BUTTON)) goto fail;
                    break;
                    case 's':
                        if(sendStatusButtonTogglePacket(EXTSYNC_BUTTON)) goto fail;
                    break;
                    case 'B':
                        if(sendStatusButtonTogglePacket(BLUE_ONLY_BUTTON)) goto fail;
                    break;
                    case 'r':
                        if(sendStatusButtonTogglePacket(R_CUTOFF_BUTTON)) goto fail;
                    break;
                    case 'g':
                        if(sendStatusButtonTogglePacket(G_CUTOFF_BUTTON)) goto fail;
                    break;
                    case 'b':
                        if(sendStatusButtonTogglePacket(B_CUTOFF_BUTTON)) goto fail;
                    break;
                    case 'K':
                        if(sendStatusButtonTogglePacket(MARKER_BUTTON)) goto fail;
                    break;
                    case 'U':
                        if(sendStatusButtonTogglePacket(CHROMA_UP_BUTTON)) goto fail;
                    break;
                    case 'H':
                        if(sendStatusButtonTogglePacket(MAN_PHASE_BUTTON)) goto fail;
                    break;
                    case 'R':
                        if(sendStatusButtonTogglePacket(MAN_CHROMA_BUTTON)) goto fail;
                    break;
                    case 'I':
                        if(sendStatusButtonTogglePacket(MAN_BRIGHT_BUTTON)) goto fail;
                    break;
                    case 'N':
                        if(sendStatusButtonTogglePacket(MAN_CONTRAST_BUTTON)) goto fail;
                    break;
                    case 'k':
                        knobselect = !knobselect;
                    break;
                    case 'q':
                        disconnect = 1;
                    break;
                    default:
                    break;
                }
            }
        }

        if(send(sockfd, get_status, sizeof(get_status), 0) < 0)
	    {
		    fprintf(stderr,"Sending get status failed...\n");
		    goto fail;
	    }
        if(recv(sockfd,status_response,sizeof(status_response),0) != sizeof(status_response)) {
		    fprintf(stderr,"Sending get status failed...\n");
		    goto fail;
        }
        statusw1 =  ((status_response[29] - 0x30) << 12) +
                    ((status_response[30] - 0x30) << 8) +
                    ((status_response[31] - 0x30) << 4) +
                    ((status_response[32] - 0x30));

        statusw2 =  ((status_response[34] - 0x30) << 12) +
                    ((status_response[35] - 0x30) << 8) +
                    ((status_response[36] - 0x30) << 4) +
                    ((status_response[37] - 0x30));

        statusw3 =  ((status_response[39] - 0x30) << 12) +
                    ((status_response[40] - 0x30) << 8) +
                    ((status_response[41] - 0x30) << 4) +
                    ((status_response[42] - 0x30));

        statusw4 =  ((status_response[44] - 0x30) << 12) +
                    ((status_response[45] - 0x30) << 8) +
                    ((status_response[46] - 0x30) << 4) +
                    ((status_response[47] - 0x30));

        statusw5 =  ((status_response[49] - 0x30) << 12) +
                    ((status_response[50] - 0x30) << 8) +
                    ((status_response[51] - 0x30) << 4) +
                    ((status_response[52] - 0x30));
        if(statusw1 & POWER_ON_STATUS) {
            if(statusw1 != _statusw1 || statusw2 != _statusw2 ||
                statusw3 != _statusw3 || statusw4 != _statusw4 ||
                statusw5 != _statusw5 || 1 == knobselect)
            {        
                printf("\rStatus: %.04X %.04X %.04X %.04X %.04X",
                    statusw1,statusw2,statusw3,statusw4,statusw5);
                if(knobselect) {
                    printf(" - *P(H)ASE *CH(R)OMA *BR(I)GHT *CO(N)TRAST                   ");
                } else {
                    printf(" - ");
                    if(currentKnob == KNOB_PHASE) printf("*");
                    printf("PHASE ");
                    if(currentKnob == KNOB_CHROMA) printf("*");
                    printf("CHROMA ");
                    if(currentKnob == KNOB_BRIGHT) printf("*");
                    printf("BRIGHT ");
                    if(currentKnob == KNOB_CONTRAST) printf("*");
                    printf("CONTRAST                    ");
                }
                fflush(stdout);
                _statusw1 = statusw1;
                _statusw2 = statusw2;
                _statusw3 = statusw3;
                _statusw4 = statusw4;
                _statusw5 = statusw5;
            } else if(knobchanged) {
                printf("\rStatus: %.04X %.04X %.04X %.04X %.04X",
                    statusw1,statusw2,statusw3,statusw4,statusw5);
                printf(" - ");
                if(currentKnob == KNOB_PHASE) printf("*");
                printf("PHASE ");
                if(currentKnob == KNOB_CHROMA) printf("*");
                printf("CHROMA ");
                if(currentKnob == KNOB_BRIGHT) printf("*");
                printf("BRIGHT ");
                if(currentKnob == KNOB_CONTRAST) printf("*");
                printf("CONTRAST                                       ");
                knobchanged = 0;
                fflush(stdout);
            }
        } else {
            printf("\rMonitor is powered off...                               \r");
            fflush(stdout);
        }
        usleep(500*1000);
    }
    goto close;

fail:
    rc = 3;

close:
    fprintf(stderr,"\nClosing connection, and exiting...\n");
    close(sockfd);
    ctrl.c_lflag |= ECHO; // turn echo back on again
    ctrl.c_lflag |= ICANON; // make input buffered again
    tcsetattr(STDIN_FILENO, TCSANOW, &ctrl);
    return rc;
}
