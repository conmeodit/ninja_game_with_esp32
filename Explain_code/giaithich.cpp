// Thêm các thư viện cần thiết
#include <Wire.h>             // Thư viện giao tiếp I2C
#include <Adafruit_GFX.h>     // Thư viện đồ họa cơ bản
#include <Adafruit_SSD1306.h> // Thư viện điều khiển màn hình OLED
#include <EEPROM.h>           // Thư viện để lưu trữ dữ liệu vào bộ nhớ không volatile

// Định nghĩa các hằng số cho màn hình
#define SCREEN_WIDTH 128    // Chiều rộng màn hình OLED (pixels)
#define SCREEN_HEIGHT 64    // Chiều cao màn hình OLED (pixels)
#define OLED_RESET -1       // Pin reset cho màn hình (-1 nếu chia sẻ với pin reset của Arduino)
#define BUTTON_JUMP 18      // Pin cho nút nhảy
#define BUTTON_DUCK 19      // Pin cho nút cúi xuống
#define EEPROM_HIGH_SCORE 0 // Địa chỉ trong EEPROM để lưu điểm cao

// Khởi tạo đối tượng màn hình OLED với các thông số đã định nghĩa
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Các hằng số vật lý cho game
const float GRAVITY = 0.6;                 // Gia tốc rơi tự do
const float JUMP_FORCE = -7.0;             // Lực nhảy (âm vì trục y hướng xuống)
const float MAX_FALL_SPEED = 12.0;         // Tốc độ rơi tối đa
const float GROUND_Y = SCREEN_HEIGHT - 16; // Vị trí mặt đất

// Mảng bitmap cho các sprite của nhân vật ninja khi chạy frame 1
// Mỗi byte đại diện cho 8 pixel, 1 là pixel sáng, 0 là pixel tối
const unsigned char PROGMEM ninja_run1[] = {
    0x06, 0xFC, 0xB5, 0x92, 0x59, 0x8A, 0x02, 0x92,
    0x62, 0x82, 0xBC, 0x7C, 0x00, 0x10, 0x31, 0xF0,
    0x3B, 0x38, 0x1E, 0x68, 0x0E, 0x4C, 0x0E, 0x44,
    0x00, 0x80, 0x01, 0x40, 0x02, 0x20, 0x04, 0x20};

// Sprite ninja chạy frame 2 - tạo hiệu ứng animation chạy
const unsigned char PROGMEM ninja_run2[] = {
    0xB2, 0x7C, 0x4D, 0x92, 0x01, 0x8A, 0x96, 0x92,
    0x68, 0x82, 0x00, 0x7C, 0x00, 0x10, 0x30, 0xF8,
    0x39, 0xAC, 0x1F, 0x64, 0x0E, 0x42, 0x0E, 0x40,
    0x00, 0x80, 0x01, 0x40, 0x01, 0x80, 0x01, 0x80};

// Sprite ninja khi nhảy
const unsigned char PROGMEM ninja_jump[] = {
    0xB2, 0x7C, 0x4D, 0x92, 0x01, 0x8A, 0x96, 0x92,
    0x68, 0x82, 0x00, 0x7C, 0x00, 0x10, 0x00, 0xF8,
    0x01, 0xAC, 0x03, 0x67, 0x00, 0x43, 0x00, 0x47,
    0x00, 0x8E, 0x01, 0x4C, 0x02, 0x20, 0x04, 0x20};

// Sprite ninja khi cúi xuống
const unsigned char PROGMEM ninja_duck[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0B, 0xBC, 0x04, 0x5C, 0x0C, 0xEE,
    0xC3, 0x11, 0xE0, 0xF1, 0x79, 0xD1, 0x3E, 0xAE,
    0x38, 0x90, 0x01, 0x48, 0x02, 0x20, 0x04, 0x20};

// Sprite cây nhỏ - chướng ngại vật
const unsigned char PROGMEM tree_small[] = {
    0x1E, 0x7E, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F,
    0x7E, 0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18};

// Sprite cây lớn - chướng ngại vật
const unsigned char PROGMEM tree_large[] = {
    0x08, 0x00, 0x1E, 0x00, 0x3F, 0x00, 0x7D, 0x80,
    0xFF, 0xC0, 0xFF, 0xE0, 0xFF, 0xE0, 0xFF, 0xF0,
    0x3F, 0xF0, 0x37, 0xF0, 0x33, 0xE0, 0x30, 0xC0,
    0x30, 0xC0, 0x30, 0xC0, 0x30, 0xC0, 0x30, 0xC0};

// Sprite phi tiêu - chướng ngại vật
const unsigned char PROGMEM kunai[] = {
    0x08, 0x00, 0x18, 0x06, 0x78, 0x09, 0xFF, 0xF9,
    0x78, 0x09, 0x18, 0x06, 0x08, 0x00, 0x00, 0x00};

// Sprite mây - phần tử trang trí
const unsigned char PROGMEM cloud[] = {
    0x00, 0x38, 0x00, 0x7C, 0x38, 0xFE, 0x7D, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Cấu trúc dữ liệu cho các đối tượng trong game
struct GameObject
{
    float x;          // Vị trí x
    float y;          // Vị trí y
    int width;        // Chiều rộng hitbox
    int height;       // Chiều cao hitbox
    float velocity_x; // Vận tốc theo trục x
    float velocity_y; // Vận tốc theo trục y
    bool active;      // Trạng thái hoạt động của đối tượng
    uint8_t type;     // Loại đối tượng (cây, phi tiêu...)
    uint8_t frame;    // Frame hiện tại của animation
};

// Định nghĩa các trạng thái của game
enum GameState
{
    MENU,     // Màn hình menu chính
    PLAYING,  // Đang trong game
    GAME_OVER // Màn hình kết thúc
};

// Định nghĩa các loại chướng ngại vật
enum ObstacleType
{
    TREE_SMALL, // Cây nhỏ
    TREE_LARGE, // Cây lớn
    KUNAI_LOW,  // Phi tiêu bay thấp
    KUNAI_HIGH  // Phi tiêu bay cao
};

// Cấu trúc dữ liệu cho sprite
struct Sprite
{
    const unsigned char *bitmap; // Con trỏ đến dữ liệu bitmap của sprite
    int width;                   // Chiều rộng sprite
    int height;                  // Chiều cao sprite
};

// Khai báo các biến toàn cục
GameObject player;                              // Đối tượng người chơi
GameObject obstacles[3];                        // Mảng các chướng ngại vật
GameObject clouds[2];                           // Mảng các đám mây
GameState gameState = MENU;                     // Trạng thái game ban đầu
unsigned long lastFrameTime = 0;                // Thời điểm vẽ frame cuối cùng
unsigned long frameCount = 0;                   // Số frame đã vẽ
unsigned long animationTimer = 0;               // Bộ đếm thời gian cho animation
int score = 0;                                  // Điểm số hiện tại
int highScore = 0;                              // Điểm cao nhất
float gameSpeed = 1.0;                          // Tốc độ game (tăng theo thời gian)
bool isNight = false;                           // Trạng thái ngày/đêm
unsigned long dayNightTimer = 0;                // Bộ đếm cho chu kỳ ngày/đêm
const unsigned long DAY_NIGHT_DURATION = 10000; // Thời gian một chu kỳ ngày/đêm (ms)

// Hàm kiểm tra giá trị pixel trong bitmap
bool getBitmapPixel(const unsigned char *bitmap, int bmpWidth, int bmpHeight, int x, int y)
{
    // Kiểm tra tọa độ có nằm trong bitmap không
    if (x < 0 || x >= bmpWidth || y < 0 || y >= bmpHeight)
        return false;
    // Tính vị trí byte chứa pixel cần kiểm tra
    int bytesPerRow = (bmpWidth + 7) / 8;
    int byteIndex = y * bytesPerRow + (x / 8);
    // Đọc giá trị byte từ bộ nhớ PROGMEM
    uint8_t byteVal = pgm_read_byte(&bitmap[byteIndex]);
    // Tính vị trí bit trong byte và trả về giá trị pixel
    int bitIndex = 7 - (x % 8);
    return (byteVal >> bitIndex) & 0x01;
}

// Hàm kiểm tra va chạm pixel-perfect giữa hai sprite
bool pixelCollision(float ax, float ay, int aw, int ah, const unsigned char *bitmapA,
                    float bx, float by, int bw, int bh, const unsigned char *bitmapB)
{
    // Tìm vùng giao nhau giữa hai sprite
    int left = max((int)ax, (int)bx);
    int right = min((int)(ax + aw), (int)(bx + bw));
    int top = max((int)ay, (int)by);
    int bottom = min((int)(ay + ah), (int)(by + bh));

    // Nếu không có vùng giao nhau, không có va chạm
    if (left >= right || top >= bottom)
        return false;

    // Kiểm tra từng pixel trong vùng giao nhau
    for (int y = top; y < bottom; y++)
    {
        for (int x = left; x < right; x++)
        {
            // Tính tọa độ tương đối trong từng sprite
            int axRel = x - (int)ax;
            int ayRel = y - (int)ay;
            int bxRel = x - (int)bx;
            int byRel = y - (int)by;

            // Kiểm tra nếu cả hai pixel đều không trong suốt
            bool pixelA = getBitmapPixel(bitmapA, aw, ah, axRel, ayRel);
            bool pixelB = getBitmapPixel(bitmapB, bw, bh, bxRel, byRel);
            if (pixelA && pixelB)
                return true;
        }
    }
    return false;
}

// Hàm lấy sprite hiện tại của người chơi
Sprite getPlayerSprite()
{
    Sprite s;
    // Chọn sprite dựa trên trạng thái người chơi
    if (digitalRead(BUTTON_DUCK) == LOW)
    {
        // Nếu đang cúi
        s.bitmap = ninja_duck;
        s.width = 16;
        s.height = 16;
    }
    else if (player.y < GROUND_Y)
    {
        // Nếu đang nhảy
        s.bitmap = ninja_jump;
        s.width = 16;
        s.height = 16;
    }
    else
    {
        // Nếu đang chạy (luân phiên 2 frame)
        s.bitmap = (player.frame ? ninja_run1 : ninja_run2);
        s.width = 16;
        s.height = 16;
    }
    return s;
}

// Hàm lấy sprite cho chướng ngại vật
// Tiếp tục hàm getObstacleSprite
Sprite getObstacleSprite(GameObject &ob)
{
    Sprite s;
    // Chọn sprite dựa trên loại chướng ngại vật
    switch (ob.type)
    {
    case TREE_SMALL: // Cây nhỏ
        s.bitmap = tree_small;
        s.width = 8;
        s.height = 16;
        break;
    case TREE_LARGE: // Cây lớn
        s.bitmap = tree_large;
        s.width = 12;
        s.height = 16;
        break;
    case KUNAI_LOW:  // Phi tiêu bay thấp
    case KUNAI_HIGH: // Phi tiêu bay cao
        s.bitmap = kunai;
        s.width = 16;
        s.height = 8;
        break;
    default: // Trường hợp không xác định
        s.bitmap = NULL;
        s.width = 0;
        s.height = 0;
    }
    return s;
}

// Hàm thiết lập ban đầu
void setup()
{
    Serial.begin(115200); // Khởi tạo giao tiếp Serial để debug

    // Thiết lập các chân GPIO
    pinMode(BUTTON_JUMP, INPUT_PULLUP); // Nút nhảy với điện trở kéo lên
    pinMode(BUTTON_DUCK, INPUT_PULLUP); // Nút cúi với điện trở kéo lên
    pinMode(BUZZER_PIN, OUTPUT);        // Chân buzzer là output

    // Khởi tạo màn hình OLED
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;)
            ; // Dừng nếu không khởi tạo được màn hình
    }

    // Khởi tạo EEPROM và đọc điểm cao
    EEPROM.begin(512);
    highScore = EEPROM.read(EEPROM_HIGH_SCORE);

    // Thiết lập màn hình
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);

    // Khởi tạo game và hiển thị màn hình chào
    initializeGame();
    showSplashScreen();
}

// Vòng lặp chính của game
void loop()
{
    unsigned long currentTime = millis();

    // Cập nhật game mỗi 16ms (khoảng 60fps)
    if (currentTime - lastFrameTime >= 16)
    {
        // Xử lý game dựa trên trạng thái hiện tại
        switch (gameState)
        {
        case MENU:
            handleMenu(); // Xử lý menu chính
            break;
        case PLAYING:
            handleGameplay(); // Xử lý gameplay
            break;
        case GAME_OVER:
            handleGameOver(); // Xử lý màn hình game over
            break;
        }

        lastFrameTime = currentTime;
        frameCount++;
    }
}

// Hàm xử lý gameplay chính
void handleGameplay()
{
    handleInput();         // Xử lý đầu vào từ người chơi
    updateGame();          // Cập nhật trạng thái game
    handleCollisions();    // Kiểm tra va chạm
    updateDayNightCycle(); // Cập nhật chu kỳ ngày đêm
    drawGame();            // Vẽ game lên màn hình
}

// Hàm cập nhật trạng thái game
void updateGame()
{
    // Xử lý vật lý cho người chơi
    if (player.y < GROUND_Y || player.velocity_y < 0)
    {
        // Áp dụng trọng lực khi đang trong không trung
        if (player.y < GROUND_Y && digitalRead(BUTTON_DUCK) == LOW)
        {
            player.velocity_y += GRAVITY * 2; // Rơi nhanh hơn khi cúi
        }
        else
        {
            player.velocity_y += GRAVITY; // Rơi bình thường
        }

        // Giới hạn tốc độ rơi tối đa
        if (player.velocity_y > MAX_FALL_SPEED)
        {
            player.velocity_y = MAX_FALL_SPEED;
        }

        // Cập nhật vị trí theo trục y
        player.y += player.velocity_y;

        // Kiểm tra và xử lý va chạm với mặt đất
        if (player.y > GROUND_Y)
        {
            player.y = GROUND_Y;
            player.velocity_y = 0;
        }
    }

    // Cập nhật animation chạy (mỗi 6 frame đổi 1 lần)
    if (player.y >= GROUND_Y && frameCount % 6 == 0)
    {
        player.frame = !player.frame;
    }

    // Cập nhật vị trí và trạng thái các chướng ngại vật
    for (int i = 0; i < 3; i++)
    {
        if (obstacles[i].active)
        {
            // Di chuyển chướng ngại vật
            obstacles[i].x += obstacles[i].velocity_x * gameSpeed;

            // Animation cho kunai
            if (obstacles[i].type == KUNAI_LOW || obstacles[i].type == KUNAI_HIGH)
            {
                if (frameCount % 10 == 0)
                {
                    obstacles[i].frame = !obstacles[i].frame;
                }
            }

            // Reset chướng ngại vật khi ra khỏi màn hình
            if (obstacles[i].x < -obstacles[i].width)
            {
                resetObstacle(i);
                score++; // Tăng điểm

                // Tăng tốc độ game sau mỗi 15 điểm
                if (score % 15 == 0)
                {
                    gameSpeed += 0.1;
                }
            }
        }
    }
    // Cập nhật vị trí các đám mây
    for (int i = 0; i < 2; i++)
    {
        if (clouds[i].active)
        {
            // Di chuyển mây sang trái
            clouds[i].x += clouds[i].velocity_x;
            // Reset mây khi ra khỏi màn hình
            if (clouds[i].x < -clouds[i].width)
            {
                clouds[i].x = SCREEN_WIDTH;
                clouds[i].y = random(0, SCREEN_HEIGHT / 2);
            }
        }
    }
}

// Hàm xử lý va chạm giữa các đối tượng
void handleCollisions()
{
    // Kiểm tra va chạm với từng chướng ngại vật
    for (int i = 0; i < 3; i++)
    {
        if (obstacles[i].active)
        {
            // Lấy sprite hiện tại của người chơi và chướng ngại vật
            Sprite playerSprite = getPlayerSprite();
            Sprite obstacleSprite = getObstacleSprite(obstacles[i]);

            // Kiểm tra va chạm pixel-perfect
            if (pixelCollision(
                    player.x, player.y,
                    playerSprite.width, playerSprite.height,
                    playerSprite.bitmap,
                    obstacles[i].x, obstacles[i].y,
                    obstacleSprite.width, obstacleSprite.height,
                    obstacleSprite.bitmap))
            {
                gameOver(); // Kết thúc game nếu có va chạm
                return;
            }
        }
    }
}

// Hàm kiểm tra va chạm đơn giản bằng hình chữ nhật
bool checkCollision(GameObject &a, GameObject &b)
{
    const int marginPlayer = 2; // Margin để va chạm mượt hơn
    const int marginObstacle = 2;

    // Kiểm tra va chạm dựa trên hitbox
    return (a.x + marginPlayer < b.x + b.width - marginObstacle &&
            a.x + a.width - marginPlayer > b.x + marginObstacle &&
            a.y + marginPlayer < b.y + b.height - marginObstacle &&
            a.y + a.height - marginPlayer > b.y + marginObstacle);
}

// Hàm vẽ game
void drawGame()
{
    display.clearDisplay(); // Xóa màn hình

    // Vẽ các ngôi sao nếu là ban đêm
    if (isNight)
    {
        for (int i = 0; i < 5; i++)
        {
            // Tạo hiệu ứng sao di chuyển
            int x = (SCREEN_WIDTH - 1) - ((frameCount * 2 + i * 30) % SCREEN_WIDTH);
            display.drawPixel(x, i * 10 + 5, WHITE);
        }
    }

    // Vẽ các đám mây
    for (int i = 0; i < 2; i++)
    {
        if (clouds[i].active)
        {
            display.drawBitmap(clouds[i].x, clouds[i].y, cloud, 16, 8, WHITE);
        }
    }

    // Vẽ đường mặt đất
    display.drawLine(0, SCREEN_HEIGHT - 1, SCREEN_WIDTH, SCREEN_HEIGHT - 1, WHITE);

    // Vẽ người chơi với sprite phù hợp
    if (digitalRead(BUTTON_DUCK) == LOW)
    {
        display.drawBitmap(player.x, player.y, ninja_duck, 16, 16, WHITE);
    }
    else if (player.y < GROUND_Y)
    {
        display.drawBitmap(player.x, player.y, ninja_jump, 16, 16, WHITE);
    }
    else
    {
        display.drawBitmap(player.x, player.y,
                           player.frame ? ninja_run1 : ninja_run2, 16, 16, WHITE);
    }

    // Vẽ các chướng ngại vật
    for (int i = 0; i < 3; i++)
    {
        if (obstacles[i].active)
        {
            switch (obstacles[i].type)
            {
            case TREE_SMALL:
                display.drawBitmap(obstacles[i].x, obstacles[i].y,
                                   tree_small, 8, 16, WHITE);
                break;
            case TREE_LARGE:
                display.drawBitmap(obstacles[i].x, obstacles[i].y,
                                   tree_large, 12, 16, WHITE);
                break;
            case KUNAI_LOW:
            case KUNAI_HIGH:
                display.drawBitmap(obstacles[i].x, obstacles[i].y,
                                   kunai, 16, 8, WHITE);
                break;
            }
        }
    }

    // Hiển thị điểm số
    display.setCursor(SCREEN_WIDTH - 40, 5);
    display.print(score);

    // Cập nhật màn hình
    display.display();
}

// Hàm xử lý đầu vào từ người chơi
void handleInput()
{
    static bool canJump = true;                        // Biến kiểm tra có thể nhảy không
    static bool jumpButtonPrevious = true;             // Trạng thái nút nhảy ở frame trước
    bool jumpButtonCurrent = digitalRead(BUTTON_JUMP); // Đọc trạng thái nút nhảy hiện tại

    // Cho phép nhảy khi chạm đất
    if (player.y >= GROUND_Y)
    {
        canJump = true;
    }

    // Xử lý nhảy khi nhấn nút
    if (jumpButtonCurrent == LOW && jumpButtonPrevious == HIGH && canJump)
    {
        player.velocity_y = JUMP_FORCE;
        canJump = false;
    }

    jumpButtonPrevious = jumpButtonCurrent;
}

// Hàm reset chướng ngại vật
void resetObstacle(int index)
{
    // Đặt vị trí mới cho chướng ngại vật
    obstacles[index].x = SCREEN_WIDTH + random(115, 150);

    // Chọn loại chướng ngại vật ngẫu nhiên
    int type = random(4);

    // Thiết lập thông số dựa trên loại
    switch (type)
    {
    case TREE_SMALL:
        obstacles[index].width = 8;
        obstacles[index].height = 16;
        obstacles[index].y = SCREEN_HEIGHT - 16;
        break;
    case TREE_LARGE:
        obstacles[index].width = 12;
        obstacles[index].height = 16;
        obstacles[index].y = SCREEN_HEIGHT - 16;
        break;
    case KUNAI_LOW:
        obstacles[index].width = 16;
        obstacles[index].height = 8;
        obstacles[index].y = SCREEN_HEIGHT - 15;
        break;
    case KUNAI_HIGH:
        obstacles[index].width = 16;
        obstacles[index].height = 8;
        obstacles[index].y = SCREEN_HEIGHT - 19;
        break;
    }

    // Cập nhật các thông số khác
    obstacles[index].type = type;
    obstacles[index].active = true;
    obstacles[index].velocity_x = -3;
    obstacles[index].frame = 0;
}

// Hàm xử lý kết thúc game
void gameOver()
{
    gameState = GAME_OVER;

    // Cập nhật điểm cao nếu cần
    if (score > highScore)
    {
        highScore = score;
        EEPROM.write(EEPROM_HIGH_SCORE, highScore);
        EEPROM.commit();
    }
}

// Hàm hiển thị màn hình chào
// Hàm hiển thị màn hình splash/intro của game
void showSplashScreen()
{
    display.clearDisplay();     // Xóa màn hình hiển thị
    display.setTextSize(2);     // Đặt kích thước chữ là 2 (lớn)
    display.setCursor(10, 5);   // Đặt vị trí con trỏ text tại x=10, y=5
    display.print("NINJA RUN"); // In ra tiêu đề game

    display.setTextSize(1);               // Đặt kích thước chữ là 1 (nhỏ)
    display.setCursor(8, 40);             // Di chuyển con trỏ xuống dưới
    display.print("Press JUMP to start"); // In hướng dẫn
    display.display();                    // Cập nhật nội dung lên màn hình

    // Đợi cho đến khi nút JUMP được nhấn
    while (digitalRead(BUTTON_JUMP) == HIGH)
    {
        delay(10); // Tạm dừng 10ms để tránh quét nút quá nhanh
    }

    gameState = PLAYING; // Chuyển trạng thái game sang PLAYING
}

// Hàm xử lý menu chính của game
void handleMenu()
{
    display.clearDisplay();     // Xóa màn hình
    display.setTextSize(2);     // Đặt cỡ chữ lớn
    display.setCursor(8, 5);    // Đặt vị trí
    display.print("NINJA RUN"); // In tiêu đề

    display.setCursor(20, 45);            // Di chuyển con trỏ
    display.print("Press JUMP to start"); // In hướng dẫn
    display.display();                    // Cập nhật màn hình

    // Kiểm tra nếu nút JUMP được nhấn
    if (digitalRead(BUTTON_JUMP) == LOW)
    {
        delay(100);          // Chờ chút để tránh dội nút
        initializeGame();    // Khởi tạo game
        gameState = PLAYING; // Chuyển sang trạng thái chơi
    }
}

// Hàm xử lý khi game kết thúc
void handleGameOver()
{
    display.clearDisplay();     // Xóa màn hình
    display.setTextSize(2);     // Đặt cỡ chữ lớn
    display.setCursor(11, 0);   // Đặt vị trí
    display.print("GAME OVER"); // In thông báo game over

    // Hiển thị điểm số
    display.setTextSize(1);   // Đặt cỡ chữ nhỏ
    display.setCursor(0, 26); // Di chuyển con trỏ
    display.print("Score: ");
    display.print(score); // In điểm số

    // Hiển thị hướng dẫn restart
    display.setCursor(0, 45);
    display.print("Press JUMP to restart");
    display.display();

    // Kiểm tra nút JUMP để restart game
    if (digitalRead(BUTTON_JUMP) == LOW)
    {
        delay(200);          // Chờ chút để tránh dội nút
        initializeGame();    // Khởi tạo lại game
        gameState = PLAYING; // Chuyển sang trạng thái chơi
    }
}

// Hàm khởi tạo game, thiết lập các giá trị ban đầu
void initializeGame()
{
    // Khởi tạo thông số người chơi
    player.x = 20;         // Vị trí x ban đầu
    player.y = GROUND_Y;   // Vị trí y (trên mặt đất)
    player.width = 16;     // Chiều rộng sprite
    player.height = 16;    // Chiều cao sprite
    player.velocity_x = 0; // Vận tốc ngang
    player.velocity_y = 0; // Vận tốc dọc
    player.active = true;  // Trạng thái hoạt động
    player.frame = 0;      // Frame animation hiện tại

    // Khởi tạo các chướng ngại vật
    for (int i = 0; i < 3; i++)
    {
        obstacles[i].active = false;             // Vô hiệu hóa
        resetObstacle(i);                        // Đặt lại vị trí
        obstacles[i].x = SCREEN_WIDTH + i * 100; // Đặt vị trí ban đầu
    }

    // Khởi tạo các đám mây
    for (int i = 0; i < 2; i++)
    {
        clouds[i].x = SCREEN_WIDTH + i * 80;        // Vị trí x
        clouds[i].y = random(0, SCREEN_HEIGHT / 2); // Vị trí y ngẫu nhiên
        clouds[i].width = 12;                       // Chiều rộng
        clouds[i].height = 6;                       // Chiều cao
        clouds[i].velocity_x = -1;                  // Tốc độ di chuyển
        clouds[i].active = true;                    // Kích hoạt
    }

    // Khởi tạo các biến khác
    score = 0;                // Đặt điểm về 0
    gameSpeed = 1.0;          // Tốc độ game ban đầu
    frameCount = 0;           // Đếm số frame
    isNight = false;          // Bắt đầu với ban ngày
    dayNightTimer = millis(); // Đặt thời gian cho chu kỳ ngày/đêm
}

// Hàm cập nhật chu kỳ ngày/đêm
void updateDayNightCycle()
{
    // Kiểm tra nếu đã đến lúc chuyển đổi ngày/đêm
    if (millis() - dayNightTimer >= DAY_NIGHT_DURATION)
    {
        isNight = !isNight;       // Đảo trạng thái ngày/đêm
        dayNightTimer = millis(); // Đặt lại thời gian
    }
}