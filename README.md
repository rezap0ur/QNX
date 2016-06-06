# QNX
API for realtime periodic task runnig over QNX Neutrino
the main aim of this project is to define an API for QNX Neutrino in which we could run periodic tasks in different ways as it would be required in Real-time operating systems and Embedded systems. 
The project has started with:

1- Rate Monotonic Periodic Task (already done, required improvements)   
2- Deadline Monotonic   
3- Earliest Deadline First    
4- Latest Deadline First  
5- Timer synchronization (problem in QNX timer which tick time is 183ns less than 1 sec)    
6- Process Utilization Bound (already done Hyperboulic Bound & Liu&Layland Bound guarantee tests )   
7- Response Time Analysis   
8- Process Demand Criterion   
