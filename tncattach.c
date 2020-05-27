#include <stdlib.h>
#include <stdbool.h>
#include <signal.h>
#include <poll.h>
#include <argp.h>
#include "Constants.h"
#include "Serial.h"
#include "KISS.h"
#include "TAP.h"

#define BAUDRATE_DEFAULT 0
#define SERIAL_BUFFER_SIZE 512

#define IF_FD_INDEX 0
#define TNC_FD_INDEX 1
#define N_FDS 2

struct pollfd fds[N_FDS];

int attached_tnc;
int attached_if;

char if_name[IFNAMSIZ];

uint8_t serial_buffer[MTU_MAX];
uint8_t if_buffer[MTU_MAX];

bool verbose = false;
bool noipv6 = false;
bool noup = false;
bool daemonize = false;
bool set_ipv4 = false;
bool set_netmask = false;
char* ipv4_addr;
char* netmask;
int mtu;
int device_type = IF_TUN;

void cleanup(void) {
	close_port(attached_tnc);
	close_tap(attached_if);
}

void signal_handler(int signal) {
	cleanup();
	exit(0);
}

bool is_ipv6(uint8_t* frame) {
	if (device_type == IF_TAP) {
		if (frame[12] == 0x86 && frame[13] == 0xdd) {
			return true;
		} else {
			return false;
		}
	} else if (device_type == IF_TUN) {
		if (frame[2] == 0x86 && frame[3] == 0xdd) {
			return true;
		} else {
			return false;
		}
	} else {
		printf("Error: Unsupported interface type\r\n");
		cleanup();
		exit(1);
	}
}

void read_loop(void) {
	bool should_continue = true;
	int min_frame_size;
	if (device_type == IF_TAP) {
		min_frame_size = ETHERNET_MIN_FRAME_SIZE;
	} else if (device_type == IF_TUN) {
		min_frame_size = TUN_MIN_FRAME_SIZE;
	} else {
		printf("Error: Unsupported interface type\r\n");
		cleanup();
		exit(1);
	}

	while (should_continue) {

		int poll_result = poll(fds, 2, -1);
		if (poll_result != -1) {
			for (int fdi = 0; fdi < N_FDS; fdi++) {
				if (fds[fdi].revents != 0) {
					// Check for hangup event
					if (fds[fdi].revents & POLLHUP) {
						if (fdi == IF_FD_INDEX) {
							printf("Received hangup from interface\r\n");
							cleanup();
							exit(1);
						}
						if (fdi == TNC_FD_INDEX) {
							printf("Received hangup from TNC\r\n");
							cleanup();
							exit(1);
						}
					}

					// Check for error event
					if (fds[fdi].revents & POLLERR) {
						if (fdi == IF_FD_INDEX) {
							perror("Received error event from interface\r\n");
							cleanup();
							exit(1);
						}
						if (fdi == TNC_FD_INDEX) {
							perror("Received error event from TNC\r\n");
							cleanup();
							exit(1);
						}
					}

					// If data is ready, read it
					if (fds[fdi].revents & POLLIN) {
						if (fdi == IF_FD_INDEX) {
							int if_len = read(attached_if, if_buffer, sizeof(if_buffer));
							if (if_len > 0) {
								if (if_len >= min_frame_size) {
									if (!noipv6 || (noipv6 && !is_ipv6(if_buffer))) {
										kiss_write_frame(attached_tnc, if_buffer, if_len);
									}
								}
							} else {
								printf("Error: Could not read from network interface, exiting now\r\n");
								cleanup();
								exit(1);
							}
						}

						if (fdi == TNC_FD_INDEX) {
							int tnc_len = read(attached_tnc, serial_buffer, sizeof(serial_buffer));
							if (tnc_len > 0) {
								if (verbose) printf("Data from TNC: %d bytes.\r\n", tnc_len);
								for (int i = 0; i < tnc_len; i++) {
									kiss_serial_read(serial_buffer[i]);
								}
							} else {
								printf("Error: Could not read from TNC, exiting now\r\n");
								cleanup();
								exit(1);
							}
						}
					}
				}
			}
		} else {
			should_continue = false;
		}
	}
	cleanup();
	exit(1);
}

const char *argp_program_version = "tncattach 0.1.1";
const char *argp_program_bug_address = "<mark@unsigned.io>";
static char doc[] = "\r\nAttach TNC devices as system network interfaces\vAs an example, to attach the TNC connected to /dev/ttyUSB0 as a full ethernet device with an MTU of 576 bytes and assign an IPv4 address, use the following command:\r\n\r\n\ttncattach /dev/ttyUSB0 115200 -m 576 -e --ipv4 10.0.0.1/24\r\n\r\nTo create an interface that doesn't use ethernet, but transports IP directly, and filters IPv6 packets out, a command like the following can be used:\r\n\r\n\ttncattach /dev/ttyUSB0 115200 --noipv6 --ipv4 10.0.0.1/24";
static char args_doc[] = "port baudrate";
static struct argp_option options[] = { 
	{ "mtu", 'm', "MTU", 0, "Specify interface MTU"},
	{ "daemon", 'd', 0, 0, "Run tncattach as a daemon"},
    { "ethernet", 'e', 0, 0, "Create a full ethernet device"},
    { "ipv4", 'i', "IP_ADDRESS", 0, "Configure an IPv4 address on interface"},
    { "noipv6", 'n', 0, 0, "Filter IPv6 traffic from reaching TNC"},
    { "noup", 1, 0, 0, "Only create interface, don't bring it up"},
    { "verbose", 'v', 0, 0, "Enable verbose output"},
    { 0 } 
};

#define N_ARGS 2
struct arguments {
	char *args[N_ARGS];
	char *ipv4;
	int baudrate;
	int mtu;
	bool tap;
	bool daemon;
	bool verbose;
	bool set_ipv4;
	bool set_netmask;
	bool noipv6;
	bool noup;
};

static error_t parse_opt(int key, char *arg, struct argp_state *state) {
	struct arguments *arguments = state->input;

	switch (key) {
		case 'v':
			arguments->verbose = true;
			break;

		case 'e':
			arguments->tap = true;
			break;

		case 'm':
			arguments->mtu = atoi(arg);
			if (arguments->mtu < MTU_MIN || arguments->mtu > MTU_MAX) {
				printf("Error: Invalid MTU specified\r\n\r\n");
				argp_usage(state);
			}
			break;

		case 'i':
			arguments->ipv4 = arg;
			arguments->set_ipv4 = true;

			if (strchr(arg, '/')) {
				char* net = strchr(arg, '/');
				int pos = net-arg;
				ipv4_addr = (char*)malloc(pos+1);
				int mask = atoi(net+1);
				strncpy(ipv4_addr, arg, pos);
				switch (mask) {
					case 0:
						netmask = "0.0.0.0";
						break;
					case 1:
						netmask = "128.0.0.0";
						break;
					case 2:
						netmask = "192.0.0.0";
						break;
					case 3:
						netmask = "224.0.0.0";
						break;
					case 4:
						netmask = "240.0.0.0";
						break;
					case 5:
						netmask = "248.0.0.0";
						break;
					case 6:
						netmask = "252.0.0.0";
						break;
					case 7:
						netmask = "254.0.0.0";
						break;
					case 8:
						netmask = "255.0.0.0";
						break;
					case 9:
						netmask = "255.128.0.0";
						break;
					case 10:
						netmask = "255.192.0.0";
						break;
					case 11:
						netmask = "255.224.0.0";
						break;
					case 12:
						netmask = "255.240.0.0";
						break;
					case 13:
						netmask = "255.248.0.0";
						break;
					case 14:
						netmask = "255.252.0.0";
						break;
					case 15:
						netmask = "255.254.0.0";
						break;
					case 16:
						netmask = "255.255.0.0";
						break;
					case 17:
						netmask = "255.255.128.0";
						break;
					case 18:
						netmask = "255.255.192.0";
						break;
					case 19:
						netmask = "255.255.224.0";
						break;
					case 20:
						netmask = "255.255.240.0";
						break;
					case 21:
						netmask = "255.255.248.0";
						break;
					case 22:
						netmask = "255.255.252.0";
						break;
					case 23:
						netmask = "255.255.254.0";
						break;
					case 24:
						netmask = "255.255.255.0";
						break;
					case 25:
						netmask = "255.255.255.128";
						break;
					case 26:
						netmask = "255.255.255.192";
						break;
					case 27:
						netmask = "255.255.255.224";
						break;
					case 28:
						netmask = "255.255.255.240";
						break;
					case 29:
						netmask = "255.255.255.248";
						break;
					case 30:
						netmask = "255.255.255.252";
						break;
					case 31:
						netmask = "255.255.255.254";
						break;
					case 32:
						netmask = "255.255.255.255";
						break;
					
					default:
						printf("Error: Invalid subnet mask specified\r\n");
						cleanup();
						exit(1);
				}

				arguments->set_netmask = true;
			} else {
				arguments->set_netmask = false;
				ipv4_addr = (char*)malloc(strlen(arg)+1);
				strcpy(ipv4_addr, arg);
			}

			break;

		case 'n':
			arguments->noipv6 = true;
			break;

		case 'd':
			arguments->daemon = true;
			arguments->verbose = false;
			break;

		case 1:
			arguments->noup = true;
			break;

		case ARGP_KEY_ARG:
			// Check if there's now too many text arguments
			if (state->arg_num >= N_ARGS) argp_usage(state);

			// If not add to args
			arguments->args[state->arg_num] = arg;
			break;

		case ARGP_KEY_END:
			// Check if there's too few text arguments
			if (state->arg_num < N_ARGS) argp_usage(state);
			break;

		default:
			return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

static struct argp argp = {options, parse_opt, args_doc, doc};
int main(int argc, char **argv) {
	struct arguments arguments;
	signal(SIGINT, signal_handler);

	arguments.baudrate = BAUDRATE_DEFAULT;
	arguments.mtu = MTU_DEFAULT;
	arguments.tap = false;
	arguments.verbose = false;
	arguments.set_ipv4 = false;
	arguments.set_netmask = false;
	arguments.noipv6 = false;
	arguments.daemon = false;
	arguments.noup = false;

	argp_parse(&argp, argc, argv, 0, 0, &arguments);
	arguments.baudrate = atoi(arguments.args[1]);

	if (arguments.daemon) daemonize = true;
	if (arguments.verbose) verbose = true;
	if (arguments.tap) device_type = IF_TAP;
	if (arguments.noipv6) noipv6 = true;
	if (arguments.set_ipv4) set_ipv4 = true;
	if (arguments.set_netmask) set_netmask = true;
	if (arguments.noup) noup = true;
	mtu = arguments.mtu;

	attached_if = open_tap();
	attached_tnc = open_port(arguments.args[0]);
	if (setup_port(attached_tnc, arguments.baudrate)) {
		printf("TNC interface configured as %s\r\n", if_name);
		fds[IF_FD_INDEX].fd = attached_if;
		fds[IF_FD_INDEX].events = POLLIN;
		fds[TNC_FD_INDEX].fd = attached_tnc;
		fds[TNC_FD_INDEX].events = POLLIN;
		read_loop();
	}
	
	return 0;
}