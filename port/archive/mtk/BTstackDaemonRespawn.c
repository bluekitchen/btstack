/** BTstack Daemon Respawn */

int main(int argc, char * argv[]){
    while (1){
        system("/system/bin/BTstackDaemon");
    }
    return 0;
}