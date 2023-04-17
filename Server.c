#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <openssl/md5.h>


#define PORT 8080
#define BUFFER_SIZE 1024

void calculate_md5(char *filename, char *md5_str) {
    unsigned char c[MD5_DIGEST_LENGTH];
    // char *filename="file.txt";
    int i;
    FILE *inFile = fopen (filename, "rb");
    MD5_CTX mdContext;
    int bytes;
    unsigned char data[1024];

    if (inFile == NULL) {
        printf ("%s can't be opened.\n", filename);
        //return 0;
    }

    MD5_Init (&mdContext);
    while ((bytes = fread (data, 1, 1024, inFile)) != 0)
    {
        MD5_Update (&mdContext, data, bytes);
    }
        
    MD5_Final (c,&mdContext);

    //char md5_str[MD5_DIGEST_LENGTH*2 + 1]; // khai báo mảng chuỗi kích thước đủ lớn
    for(i = 0; i < MD5_DIGEST_LENGTH; i++) {
        sprintf(&md5_str[i*2], "%02x", c[i]); // chuyển đổi từng phần tử unsigned char sang chuỗi hexa và nối chúng lại
    }
    md5_str[MD5_DIGEST_LENGTH*2] = '\0'; // thêm kí tự null terminator

    //printf ("%s %s\n", md5_str, filename); // xuất chuỗi MD5 và tên file
    md5_str[i*2] = '\0';
    
    fclose (inFile);
}

int main(int argc, char const *argv[]) {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};
    char message[BUFFER_SIZE];
    int num_requests = 0;

    // Tạo socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Đặt các tùy chọn cho socket
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Gán địa chỉ cho socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    // Lắng nghe kết nối từ Client
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", PORT);

    while(1) {
        // Chấp nhận kết nối mới
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        printf("Handling request #%d from %s:%d\n", ++num_requests, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

        // Xử lý request từ Client

        int num_bytes_received = recv(new_socket, buffer, BUFFER_SIZE, 0);
        
        if(num_bytes_received == -1 ) {
            printf("Failed");
            exit(EXIT_FAILURE);
        }
        else {
            printf("%s\n", buffer);
        }
        // clear response buffer
        memset(buffer, 0, BUFFER_SIZE);

        send(new_socket, "File name:", strlen("File name:"), 0);
        // clear message buffer
        memset(message, 0, BUFFER_SIZE);

        // nhận tên file mà client gửi đến
        char filename[BUFFER_SIZE];
        memset(filename, 0, BUFFER_SIZE);
        recv(new_socket, filename, BUFFER_SIZE, 0);
        printf("File is requested to send: %s\n",filename);
        memset(buffer, 0, BUFFER_SIZE);

        // gửi thông tin file cho Client
        char md5_str[MD5_DIGEST_LENGTH*2 +1];
        int file_descriptor = open(filename, O_RDONLY);
        if (file_descriptor == -1) {
            perror("Failed to open file");
            exit(EXIT_FAILURE);
        }

        int file_size = lseek(file_descriptor, 0, SEEK_END);
        lseek(file_descriptor, 0, SEEK_SET);
        calculate_md5(filename, md5_str);
        sprintf(buffer, "%s,%d,%s", filename, file_size, md5_str);

        

        int bytes = send(new_socket, buffer, strlen(buffer), 0);
        
        memset(buffer, 0, BUFFER_SIZE);

        // nhận message : "Please send file";
        recv(new_socket, buffer, BUFFER_SIZE, 0);
        printf("Client response: %s\n",buffer);

        memset(buffer, 0, BUFFER_SIZE);
        
        //gửi file đến client
        off_t offset = 0;
        ssize_t sent_bytes = 0;
        while ((sent_bytes = sendfile(new_socket, file_descriptor, &offset, BUFFER_SIZE)) > 0) {
            printf("Sent %d bytes of file\n", sent_bytes);
        }

        //nhận phản hồi từ Client xem đã dowload File thành công hay chưa
        recv(new_socket, buffer, BUFFER_SIZE, 0);

        printf("Client response: %s\n",buffer);
        memset(buffer, 0, BUFFER_SIZE);
        if(strcmp(buffer,"Dowload Done") == 0) {
                close(server_fd);
        }
        close(new_socket);
    }
    return 0;
}

