# SAMR30_P2P_Pingpong_Test
The test will check how long will spend for ATSAMR30 send data through the air, the test will using two SAMR30 node, and using the different LED to indicate the package send and receive.
The sender node will send out an package to the receiver node, and the receiver node will send back an same length but different content packet to the receiver immediately.

- Sender will toggle LED1(PA18), set 0 when send out test message, and set 1 when get echo back from receiver.
- Receiver will toggle LED0(PA19) when receive data.

Test enviroment by using two SAM R30 Xplained pro
![image](https://user-images.githubusercontent.com/20182981/119264448-d7747200-bc15-11eb-8cf3-0ee96a7d6896.png)


20 bytes payload, sender to receiver spend 15ms, and receive back spend 116ms
![image](https://user-images.githubusercontent.com/20182981/119264088-4a7ce900-bc14-11eb-8ed2-f86da9ec175d.png)

57 bytes payload, sender to receiver spend 23ms, and receive back spend 124ms
![image](https://user-images.githubusercontent.com/20182981/119264039-23beb280-bc14-11eb-8aaa-2b9a0ccac338.png)

