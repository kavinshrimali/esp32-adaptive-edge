# esp32-adaptive-edge
An energy-efficient ESP-32 IoT edge vision node that dynamically adjusts its frame rate and sleep states based on PIR-interrupts and real-time pixel delta tracking.

General Description:
In large IoT networks, excess power is consumed by components that aren't in active use. This is a particularly important issue to consider when dealing with edge systems that locally process information, for which efficient power management is crucial. Standard motion-powered systems are typically binary (ON or OFF), and lack numerous states to account for varying motion complexity. In this system, a PIR sensor acts as an external interrupt; the ESP32 remains in low-power mode (i.e. light sleep) until motion is detected. Once motion is detected, the ESP32 camera is 'woken up' and carries out pixel delta tracking by continuously processing images received from the camera. The program dynamically adjusts the camera's frame rate based on the motion's complexity (ranging from 0 ms to 300 ms), decaying back to light sleep when there's a pause of 5000 ms between consecutive image captures. 

State transitions:
IDLE: Light sleep mode, low power consumption. System awaits a PIR interrupt.
IDLE -> ACTIVE: State switches from IDLE to ACTIVE when motion is detected.
ACTIVE: Images captured by the camera are processed as a 1D array. Frame rate dynamically adjusted based on determined complexity.
ACTIVE -> DECAY: State switches from ACTIVE to DECAY when a gap of 5000 ms occurs between consecutive movements.
DECAY: The ESP32 camera remains awake. If motion is detected, the state switches back to DECAY. Otherwise, after 3000 ms the state changes to IDLE.
DECAY -> IDLE: The ESP32 camera returns to its light sleep mode.

Breakdown of the algorithm:
Hardware interrupts occur when the PIR sensor detects motion. Images captured by the ESP32 are processed as 1D arrays of bytes. If the current image being processed is the first frame, the bytes are stored in an array and no delta calculation takes place. The second frame's bytes are subtracted from the bytes from the first image (stored in the aforementioned array), and are then stored in the array. This process is repeated from the second frame onwards. If the total delta is greater than the HIGH_MOTION_THRESHOLD, the delay between image captures is set to 0 ms. If the total difference is less than this threshold, but greater than the LOW_MOTION_THRESHOLD, the delay between image captures is set to 150 ms. Otherwise, the delay is set to 300 ms. The HIGH_MOTION_THRESHOLD and LOW_MOTION_THRESHOLD were determined by aiming the ESP32 at a static frame and calculating the total 'noise' for various levels of movement (i.e. the total delta between the pixels in an array representing an image). The value determined when 0 motion occurred was deemed the LOW_MOTION_THRESHOLD; similarly, the value calculated when complex movement took place was deemed the HIGH_MOTION_THRESHOLD.

Key design decisions:
The 2 main options for the ESP32's framesize are: VGA (640 x 480) and QQVGA (160 x 120). Rapidly changing the framesize runs the risk of corrupting images. This is because, during the camera's configuration we define a framesize for the camera. Accordingly, the ESP32's internal DMA (Digital Memory Access, which is responsible for transferring bits captured by the camera to the board's peripherals without involvement of the CPU to prevent CPU bottlenecks) creates a linked list that allows for data transfer appropriate for the specified framesize, creating issues when the framesize is dynamically changed. 

I decided to use PSRAM (Pseudo-Static RAM) rather than DRAM (Data RAM) for this project to allow for continuous motion capture. PSRAM-enabled ESP32 modules are capable of storing 2 images at a time; hence, while calculating the pixel delta for the first image, the ESP32 camera takes another picture, allowing for smoother processing. Conversely, the DRAM only allows for the storage of one photo at a time, resulting in slower processing and potential delays in state transitions.  

A VGA image has 640 x 480 = 307,200 pixels. Processing each pixel in the image would be slow; to reduce the time taken to carry out the pixel delta calculation, I decided to establish a 'stride'. The 'stride' serves as the increment in the loop that iterates over each pixel in the array, and reduces the time complexity of the pixel delta calculation by a factor of 1/stride. I used a stride of 16 to concurrently achieve faster processing while ensuring that a representative sample of pixels is used in the delta calculation.

Component List:
AM312 PIR Motion Sensor
AI Thinker ESP32 Camera
ESP32 MB Camera Shield
Breadboard
Micro-USB to USB Connector
100 μF Capacitor
Jumper Cables

Pinout:

