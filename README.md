# SAMR30_P2P_Pingpong_Test
The test will check how long will spend for ATSAMR30 send data through the air, the test will using two SAMR30 node, and using the different LED to indicate the package send and receive.
The sender node will send out an package to the receiver node, and the receiver node will send back an same length but different content packet to the receiver immediately.

- Sender will toggle LED1(PA18), set 0 when send out test message, and set 1 when get echo back from receiver.
- Receiver will toggle LED0(PA19) when receive data.

Test enviroment by using two SAM R30 Xplained pro
![image](https://user-images.githubusercontent.com/20182981/119264448-d7747200-bc15-11eb-8cf3-0ee96a7d6896.png)


20 bytes payload, sender to receiver spend 1.04ms, and receive back spend 2.08ms
![image](https://user-images.githubusercontent.com/20182981/119370314-ba0fd880-bce7-11eb-825f-04e89f0f0af1.png)

99 bytes payload, sender to receiver spend 2.2ms, and receive back spend 4.4ms
![image](https://user-images.githubusercontent.com/20182981/119370083-787f2d80-bce7-11eb-9611-5bfa090b4f8a.png)
