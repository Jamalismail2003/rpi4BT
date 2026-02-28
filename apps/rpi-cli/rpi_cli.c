#include <devctl.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <unistd.h>

#include <rpi4bt/rpi4bt_msg.h>

static const char DEV_NAME[] = "/dev/rpiCTRL";

static void show_help(void)
{
    printf("Available commands:\n");
    printf("  help              Show this help message\n");
    printf("  scan              Scan for available devices\n");
    printf("  list              List available devices\n");
    printf("  pair <mac>        Pair with a device using its MAC address\n");
    printf("  sdp <mac> <uuid>  Request SDP from remote device\n");
    printf("                    UUID values: 0x0001 0x0003 0x0100 0x1101\n");
    printf("  status            Show current stack status\n");
    printf("  quit              Exit the application\n");
}

static int send_command(int btfd, custom_msg_t *msg, char *resp, size_t resp_size)
{
    iov_t input_iov[1];
    iov_t output_iov[2];
    int ret = -1;

    SETIOV(&input_iov[0], msg, sizeof(*msg));
    SETIOV(&output_iov[0], msg, sizeof(*msg));
    SETIOV(&output_iov[1], resp, resp_size);

    int status = devctlv(btfd, RPI4_CUSTOM_COMMAND, 1, 2, input_iov, output_iov, &ret);
    if (status != EOK) {
        perror("devctlv");
        return -1;
    }

    return 0;
}

static void process_command(int btfd, const char *arg, char *resp, size_t resp_size)
{
    custom_msg_t msg = {0};

    if (strcmp(arg, "help") == 0) {
        show_help();
    } else if (strcmp(arg, "scan") == 0) {
        msg.cmd = CMD_SCAN;
        if (!send_command(btfd, &msg, resp, resp_size)) {
            printf("%s\n", resp);
        }
    } else if (strcmp(arg, "status") == 0) {
        msg.cmd = CMD_STATUS;
        if (!send_command(btfd, &msg, resp, resp_size)) {
            printf("%s\n", resp);
        }
    } else if (strcmp(arg, "list") == 0) {
        msg.cmd = CMD_LIST;
        if (!send_command(btfd, &msg, resp, resp_size)) {
            printf("%s\n", resp);
        }
    } else if (strncmp(arg, "pair ", 5) == 0) {
        if (strlen(arg + 5) != 17) {
            printf("Error: invalid MAC format. Use XX:XX:XX:XX:XX:XX\n");
            return;
        }

        msg.cmd = CMD_PAIR;
        strncpy(msg.mac_addr, arg + 5, sizeof(msg.mac_addr) - 1);
        if (!send_command(btfd, &msg, resp, resp_size)) {
            printf("%s\n", resp);
        }
    } else if (strncmp(arg, "sdp ", 4) == 0) {
        if (strlen(arg + 4) != (17 + 7)) {
            printf("Error: invalid SDP command\n");
            printf("Example: sdp XX:XX:XX:XX:XX:XX 0x0003\n");
            return;
        }

        char *endptr = NULL;
        uint16_t sdp_type = (uint16_t)strtol(arg + 4 + 17 + 1, &endptr, 16);
        if (!(sdp_type == 0x0001 || sdp_type == 0x0003 || sdp_type == 0x0100 || sdp_type == 0x1101)) {
            printf("Error: unsupported UUID (use 0x0001, 0x0003, 0x0100, 0x1101)\n");
            return;
        }

        msg.cmd = CMD_SDP;
        msg.sdp_request = sdp_type;
        strncpy(msg.mac_addr, arg + 4, sizeof(msg.mac_addr) - 1);
        if (!send_command(btfd, &msg, resp, resp_size)) {
            printf("SDP result:\n%s\n", resp);
            if (sdp_type == 0x1101) {
                printf("rf_channel_num: %u\n", msg.rf_channel_num);
            }
        }
    } else {
        printf("Unknown command: %s\n", arg);
        printf("Type 'help' for available commands.\n");
    }
}

int main(void)
{
    char input[256];
    size_t resp_size = 32768;
    char *resp = NULL;

    printf("Bluetooth CLI\n");
    printf("Type 'help' for available commands.\n");

    int btfd = open(DEV_NAME, O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (btfd == -1) {
        perror("open");
        return 1;
    }

    resp = malloc(resp_size);
    if (!resp) {
        perror("malloc");
        close(btfd);
        return 1;
    }

    while (1) {
        printf("> ");
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        input[strcspn(input, "\n")] = '\0';
        if (strcmp(input, "quit") == 0) {
            break;
        }

        process_command(btfd, input, resp, resp_size);
    }

    free(resp);
    close(btfd);
    return 0;
}
