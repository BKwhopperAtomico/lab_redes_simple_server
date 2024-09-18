#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <dirent.h>

#define BUFLEN 4096  // Max length of buffer
#define PORT   8000  // The port on which to listen for incoming data

char http_ok[] = "HTTP/1.0 200 OK\r\nContent-type: text/html\r\nServer: Test\r\n\r\n";
char http_error[] = "HTTP/1.0 400 Bad Request\r\nContent-type: text/html\r\nServer: Test\r\n\r\n";

void die(char *s) {
    perror(s);
    exit(1);
}

void get_cpu_usage(char *cpu_usage) {
    FILE *file;
    char buffer[256];
    unsigned long long int user, nice, system, idle;

    file = fopen("/proc/stat", "r");
    if (file == NULL) {
        strcpy(cpu_usage, "erro na abertura do arquivo");
        return;
    }

    fgets(buffer, sizeof(buffer), file);
    sscanf(buffer, "cpu %llu %llu %llu %llu", &user, &nice, &system, &idle);
    fclose(file);

    unsigned long long total_time = user + nice + system + idle;
    unsigned long long busy_time = user + nice + system;
    double usage = ((double)busy_time / total_time) * 100;

    snprintf(cpu_usage, 256, "%.2f%%", usage);
}

void get_memory_usage(char *mem_usage) {
    FILE *file;
    char buffer[256];
    unsigned long total_mem, free_mem, buffers, cached;

    file = fopen("/proc/meminfo", "r");
    if (file == NULL) {
        strcpy(mem_usage, "erro na abertura do arquivo");
        return;
    }

    while (fgets(buffer, sizeof(buffer), file)) {
        if (sscanf(buffer, "MemTotal: %lu", &total_mem) == 1) {
            total_mem /= 1024; // Convert to MB
        } else if (sscanf(buffer, "MemFree: %lu", &free_mem) == 1) {
            free_mem /= 1024;
        } else if (sscanf(buffer, "Buffers: %lu", &buffers) == 1) {
            buffers /= 1024;
        } else if (sscanf(buffer, "Cached: %lu", &cached) == 1) {
            cached /= 1024;
        }
    }
    fclose(file);

    unsigned long used_mem = total_mem - (free_mem + buffers + cached);
    snprintf(mem_usage, 256, "%lu MB", used_mem);
}

void get_process_list(char *process_list) {
    DIR *dir;
    struct dirent *entry;
    char path[256], cmdline[256];
    FILE *file;
    int pid;

    dir = opendir("/proc");
    if (dir == NULL) {
        strcpy(process_list, "erro na abertura do arquivo");
        return;
    }

    strcat(process_list, "<ul>");
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_type == DT_DIR && sscanf(entry->d_name, "%d", &pid) == 1) {
            snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
            file = fopen(path, "r");
            if (file) {
                fgets(cmdline, sizeof(cmdline), file);
                fclose(file);
                if (strlen(cmdline) > 0) {
                    snprintf(path, sizeof(path), "<li>PID: %d - %s</li>", pid, cmdline);
                    strcat(process_list, path);
                }
            }
        }
    }
    strcat(process_list, "</ul>");
    closedir(dir);
}

void get_disk_info(char *disk_info) {
    FILE *file;
    char buffer[256];

    file = popen("df -h", "r");
    if (file == NULL) {
        strcpy(disk_info, "erro na abertura do arquivo");
        return;
    }

    strcat(disk_info, "<pre>");
    while (fgets(buffer, sizeof(buffer), file)) {
        strcat(disk_info, buffer);
    }
    strcat(disk_info, "</pre>");
    pclose(file);
}

// pasta /bus/usb nao esta sendo montada e comando usbfs/lsusb retorna nada
// perguntar outra forma de pegar info dos dispositivos USB
void get_usb_info(char *usb_info) {
    FILE *file;
    char buffer[256];

    file = popen("lsusb", "r");
    if (file == NULL) {
        strcpy(usb_info, "um erro aconteceu");
        return;
    }

    strcat(usb_info, "<pre>");
    while (fgets(buffer, sizeof(buffer), file)) {
        strcat(usb_info, buffer);
    }
    strcat(usb_info, "</pre>");
    pclose(file);
}

void get_network_adapters(char *net_info) {
    FILE *file;
    char buffer[256];

    file = popen("ip addr", "r");
    if (file == NULL) {
        strcpy(net_info, "erro na abertura do arquivo");
        return;
    }

    strcat(net_info, "<pre>");
    while (fgets(buffer, sizeof(buffer), file)) {
        strcat(net_info, buffer);
    }
    strcat(net_info, "</pre>");
    pclose(file);
}

void get_system_info(char *response) {
    char cpu_usage[256] = "";
    char mem_usage[256] = "";
    char process_list[4096] = "";
    char disk_info[4096] = "";
    char usb_info[4096] = "";
    char net_info[4096] = "";
    char time_str[64], cpu_model[256] = "Unknown", version[256] = "Unknown";
    double uptime = 0;


    // Get CPU, memory, and other system info
    get_cpu_usage(cpu_usage);
    get_memory_usage(mem_usage);
    get_process_list(process_list);
    get_disk_info(disk_info);
    get_usb_info(usb_info);
    get_network_adapters(net_info);

// Get current time
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", t);

    // uptime do sistema
    FILE *uptime_file = fopen("/proc/uptime", "r");
    if (uptime_file != NULL) {
        fscanf(uptime_file, "%lf", &uptime);
        fclose(uptime_file);
    }

    // modelo CPU 
    FILE *cpuinfo_file = fopen("/proc/cpuinfo", "r");
    char line[256];
    while (cpuinfo_file != NULL && fgets(line, sizeof(line), cpuinfo_file)) {
        if (strncmp(line, "model name", 10) == 0) {
            sscanf(line, "model name : %[^\n]", cpu_model);
            break;
        }
    }
    if (cpuinfo_file) fclose(cpuinfo_file);

    // versao do sistema
    FILE *version_file = fopen("/proc/version", "r");
    if (version_file != NULL) {
        fgets(version, sizeof(version), version_file);
        fclose(version_file);
    }

/*
<head>
<title>
System Info
</title>
</head>
<body>
<h1>System Information</h1>
<p><strong>Date and Time:</strong> 2024-09-16 23:58:56</p>
<p><strong>Uptime:</strong> 142.78 seconds</p>
<p><strong>Processor:</strong> QEMU Virtual CPU version 2.5+</p>
<p><strong>OS Version:</strong> Linux version 5.15.18 (codespace@codespaces-b07284) (i686-buildroot-linux-gnu-gcc.br_real (Buildroot -g971bf1f768-dirty) 11.3.0, GNU ld (GNU Binutils) 2.38) #1 SMP Thu Aug 29 23:14:18 UTC 2024
</p>
</body>
</html>
*/
        // BUFLEN =  4096

    // <title><System Info>\n</title>\n</head>\n<body>\n
        // info do sistema
    // "<HEADER>[TIPO DE INFO]<HEADER>\n"
    // "<p><strong>[VARIAVLE]</strong>\n"

    // mensagem HTML para requisicao GET
    snprintf(response, BUFLEN,
        "<html>\n<head>\n<title>\nSystem Info\n</title>\n</head>\n<body>\n" 
        "<h1>Informacao do Sistema</h1>\n"
        "<p><strong>Data e hora do sistema:</strong> %s</p>\n"                  // time_str
        "<p><strong>Uptime:</strong> %.2f seconds</p>\n"                        // uptime
        "<p><strong>Modelo do processador e velocidade:</strong> %s</p>\n"      // cpu_model
        "<p><strong>Capacidade ocupada do processador(\%):</strong> %s</p>\n"  // cpu_usage
        "<p><strong>Quantidade de memoria RAM (mb):</strong> %s</p>\n"          // mem_usage
        "<p><strong>Versao do Sistema:</strong> %s</p>\n"                       // version 
        "<h2>Lista de processos em execucao (PID e nome):</h2>\n%s\n"           // process_list
        "<h2>Lista de unidade de disco:</h2>\n%s\n"                             // disk_info
        "<h2>Lista de dispositivos USB:</h2>\n%s\n"                             // usb_info
        "<h2>Lista de adaptadores de rede:</h2>\n%s\n"                          // net_info
        "</body>\n</html>\n", 
        time_str, uptime, cpu_model, cpu_usage, mem_usage, version, process_list, disk_info, usb_info, net_info);

}

int main(void) {
    struct sockaddr_in si_me, si_other;

    int s, slen = sizeof(si_other), conn, recv_len;
    char buf[BUFLEN], response[BUFLEN];

    // Create a TCP socket
    if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
        die("socket");

    // Zero out the structure
    memset((char *) &si_me, 0, sizeof(si_me));

    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(PORT);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind socket to port
    if (bind(s, (struct sockaddr*)&si_me, sizeof(si_me)) == -1)
        die("bind");

    // Allow 10 requests to queue up
    if (listen(s, 10) == -1)
        die("listen");

    // Keep listening for data
    while (1) {
        memset(buf, 0, sizeof(buf));
        printf("Waiting for a connection...");
        fflush(stdout);

        conn = accept(s, (struct sockaddr *) &si_other, &slen);
        if (conn < 0)
            die("accept");

        printf("Client connected: %s:%d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));

        // Try to receive some data, this is a blocking call
        recv_len = read(conn, buf, BUFLEN);
        if (recv_len < 0)
            die("read");

        // Print details of the client/peer and the data received
        printf("Data: %s\n", buf);

        if (strstr(buf, "GET")) {
            
            get_system_info(response);

            if (write(conn, http_ok, strlen(http_ok)) < 0)
                die("write");
            if (write(conn, response, strlen(response)) < 0)
                die("write");
        } else {
            if (write(conn, http_error, strlen(http_error)) < 0)
                die("write");
        }

        close(conn);
    }
    close(s);

    return 0;
}