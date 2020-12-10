# CS120
 
## prerequisites

- install `libcap` and `libnet` for ip package sniffing and injecting

  - ubuntu or debian
  
    ```bash
    $ sudo apt install libpcap-dev
    $ sudo apt install libnet1-dev
    ```

  - macos
 
    ```bash
    $ brew install libpcap
    $ brew install libnet
    ```

- firewall settings

  - macos: disable default behavior of icmp, udp, and tcp.

    ```bash
    $ sudo ./script/nat_init.sh
    ```
