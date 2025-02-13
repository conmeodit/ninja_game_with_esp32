#include <Wire.h>             // Thư viện giao tiếp I2C, cần thiết cho màn hình OLED
#include <Adafruit_GFX.h>     // Thư viện đồ họa cơ bản của Adafruit
#include <Adafruit_SSD1306.h> // Thư viện điều khiển màn hình OLED SSD1306
#include <EEPROM.h>           // Thư viện EEPROM để lưu trữ dữ liệu (ví dụ: high score)

// Định nghĩa các thông số cơ bản cho màn hình OLED và nút bấm
#define SCREEN_WIDTH 128    // Chiều rộng màn hình OLED (128 pixel)
#define SCREEN_HEIGHT 64    // Chiều cao màn hình OLED (64 pixel)
#define OLED_RESET -1       // Chân reset của OLED (không sử dụng nên đặt -1)
#define BUTTON_JUMP 18      // Chân kết nối nút nhảy
#define BUTTON_DUCK 19      // Chân kết nối nút cúi
#define EEPROM_HIGH_SCORE 0 // Địa chỉ trong EEPROM để lưu high score

// Khởi tạo đối tượng màn hình OLED với các thông số đã định nghĩa
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Định nghĩa các hằng số vật lý của trò chơi
const float GRAVITY = 0.6;                 // Gia tốc trọng trường tác động lên nhân vật (khi rơi)
const float JUMP_FORCE = -7.0;             // Lực nhảy (giá trị âm để di chuyển lên trên)
const float MAX_FALL_SPEED = 12.0;         // Vận tốc rơi tối đa của nhân vật
const float GROUND_Y = SCREEN_HEIGHT - 16; // Vị trí của mặt đất trên màn hình (cách đáy 16 pixel)

// Định nghĩa bitmap sprite của nhân vật và các đối tượng (lưu trong bộ nhớ flash với PROGMEM)
const unsigned char PROGMEM ninja_run1[] = {
    0x06, 0xFC, 0xB5, 0x92, 0x59, 0x8A, 0x02, 0x92,
    0x62, 0x82, 0xBC, 0x7C, 0x00, 0x10, 0x31, 0xF0,
    0x3B, 0x38, 0x1E, 0x68, 0x0E, 0x4C, 0x0E, 0x44,
    0x00, 0x80, 0x01, 0x40, 0x02, 0x20, 0x04, 0x20};

const unsigned char PROGMEM ninja_run2[] = {
    0xB2, 0x7C, 0x4D, 0x92, 0x01, 0x8A, 0x96, 0x92,
    0x68, 0x82, 0x00, 0x7C, 0x00, 0x10, 0x30, 0xF8,
    0x39, 0xAC, 0x1F, 0x64, 0x0E, 0x42, 0x0E, 0x40,
    0x00, 0x80, 0x01, 0x40, 0x01, 0x80, 0x01, 0x80};

const unsigned char PROGMEM ninja_jump[] = {
    0xB2, 0x7C, 0x4D, 0x92, 0x01, 0x8A, 0x96, 0x92,
    0x68, 0x82, 0x00, 0x7C, 0x00, 0x10, 0x00, 0xF8,
    0x01, 0xAC, 0x03, 0x67, 0x00, 0x43, 0x00, 0x47,
    0x00, 0x8E, 0x01, 0x4C, 0x02, 0x20, 0x04, 0x20};

const unsigned char PROGMEM ninja_duck[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x0B, 0xBC, 0x04, 0x5C, 0x0C, 0xEE,
    0xC3, 0x11, 0xE0, 0xF1, 0x79, 0xD1, 0x3E, 0xAE,
    0x38, 0x90, 0x01, 0x48, 0x02, 0x20, 0x04, 0x20};

const unsigned char PROGMEM tree_small[] = {
    0x1E, 0x7E, 0xFE, 0xFF, 0xFF, 0xFF, 0xFF, 0x7F,
    0x7E, 0x3C, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18};

const unsigned char PROGMEM tree_large[] = {
    0x08, 0x00, 0x1E, 0x00, 0x3F, 0x00, 0x7D, 0x80, 0xFF, 0xC0, 0xFF, 0xE0, 0xFF, 0xE0, 0xFF, 0xF0,
    0x3F, 0xF0, 0x37, 0xF0, 0x33, 0xE0, 0x30, 0xC0, 0x30, 0xC0, 0x30, 0xC0, 0x30, 0xC0, 0x30, 0xC0};

const unsigned char PROGMEM kunai[] = {
    0x08, 0x00, 0x18, 0x06, 0x78, 0x09, 0xFF, 0xF9,
    0x78, 0x09, 0x18, 0x06, 0x08, 0x00, 0x00, 0x00};

const unsigned char PROGMEM cloud[] = {
    0x00, 0x38, 0x00, 0x7C, 0x38, 0xFE, 0x7D, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Định nghĩa cấu trúc GameObject dùng để đại diện cho các đối tượng trong game:
// nhân vật, chướng ngại vật, mây,…
struct GameObject
{
    float x;          // Tọa độ x của đối tượng
    float y;          // Tọa độ y của đối tượng
    int width;        // Chiều rộng của đối tượng (dùng cho va chạm, vẽ ảnh,…)
    int height;       // Chiều cao của đối tượng
    float velocity_x; // Vận tốc theo hướng x (di chuyển ngang)
    float velocity_y; // Vận tốc theo hướng y (di chuyển dọc)
    bool active;      // Cờ đánh dấu đối tượng có đang hoạt động hay không
    uint8_t type;     // Loại đối tượng (sử dụng để xác định loại sprite, ví dụ: cây, kunai,…)
    uint8_t frame;    // Frame của sprite, dùng cho hoạt ảnh
};

// Các trạng thái của trò chơi
enum GameState
{
    MENU,     // Màn hình menu (bắt đầu trò chơi)
    PLAYING,  // Trạng thái đang chơi game
    GAME_OVER // Trạng thái game kết thúc
};

// Các loại chướng ngại vật có thể xuất hiện
enum ObstacleType
{
    TREE_SMALL, // Cây nhỏ
    TREE_LARGE, // Cây lớn
    KUNAI_LOW,  // Kunai xuất hiện ở vị trí thấp
    KUNAI_HIGH  // Kunai xuất hiện ở vị trí cao
};

// Cấu trúc Sprite dùng để lưu thông tin về bitmap, chiều rộng và chiều cao của sprite
struct Sprite
{
    const unsigned char *bitmap; // Con trỏ đến mảng bitmap của sprite
    int width;                   // Chiều rộng của sprite
    int height;                  // Chiều cao của sprite
};

// Khai báo đối tượng cho nhân vật chính
GameObject player;
// Mảng chứa các đối tượng chướng ngại vật (3 đối tượng)
GameObject obstacles[3];
// Mảng chứa các đối tượng mây (2 đối tượng)
GameObject clouds[2];
// Trạng thái trò chơi hiện tại, khởi tạo ở trạng thái MENU
GameState gameState = MENU;

// Các biến toàn cục dùng để theo dõi thời gian, frame, điểm số và tốc độ trò chơi
unsigned long lastFrameTime = 0;                // Thời điểm frame cuối cùng được cập nhật
unsigned long frameCount = 0;                   // Số lượng frame đã chạy
unsigned long animationTimer = 0;               // Bộ đếm thời gian cho hoạt ảnh
int score = 0;                                  // Điểm số hiện tại của trò chơi
int highScore = 0;                              // Điểm cao nhất (được lưu trong EEPROM)
float gameSpeed = 1.0;                          // Tốc độ trò chơi (có thể tăng dần khi đạt điểm cao)
bool isNight = false;                           // Cờ đánh dấu trạng thái ban đêm (true) hay ban ngày (false)
unsigned long dayNightTimer = 0;                // Bộ đếm thời gian chuyển đổi giữa ban ngày và ban đêm
const unsigned long DAY_NIGHT_DURATION = 10000; // Thời gian (ms) cho mỗi chu kỳ ban ngày/ban đêm

// Các định nghĩa liên quan đến loa (buzzer) để phát âm thanh
const int BUZZER_PIN = 25;     // Chân kết nối buzzer
const int SOUND_JUMP = 1;      // Âm thanh khi nhảy
const int SOUND_COLLISION = 2; // Âm thanh khi va chạm
const int SOUND_POINT = 3;     // Âm thanh khi đạt điểm

// Hàm getBitmapPixel: Kiểm tra pixel tại vị trí (x, y) của bitmap có được bật không
bool getBitmapPixel(const unsigned char *bitmap, int bmpWidth, int bmpHeight, int x, int y)
{
    // Nếu tọa độ nằm ngoài kích thước bitmap, trả về false
    if (x < 0 || x >= bmpWidth || y < 0 || y >= bmpHeight)
        return false;
    // Tính số byte mỗi hàng của bitmap (mỗi byte chứa 8 pixel)
    int bytesPerRow = (bmpWidth + 7) / 8;
    // Tính chỉ số byte chứa pixel cần kiểm tra
    int byteIndex = y * bytesPerRow + (x / 8);
    // Đọc giá trị của byte từ bộ nhớ flash
    uint8_t byteVal = pgm_read_byte(&bitmap[byteIndex]);
    // Tính vị trí bit trong byte (bit trái nhất có chỉ số 7)
    int bitIndex = 7 - (x % 8);
    // Trả về true nếu bit tương ứng bằng 1, ngược lại trả về false
    return (byteVal >> bitIndex) & 0x01;
}

// Hàm pixelCollision: Kiểm tra va chạm "pixel-perfect" giữa hai sprite
bool pixelCollision(float ax, float ay, int aw, int ah, const unsigned char *bitmapA,
                    float bx, float by, int bw, int bh, const unsigned char *bitmapB)
{
    // Xác định vùng giao nhau giữa hai sprite dựa trên tọa độ và kích thước của chúng
    int left = max((int)ax, (int)bx);
    int right = min((int)(ax + aw), (int)(bx + bw));
    int top = max((int)ay, (int)by);
    int bottom = min((int)(ay + ah), (int)(by + bh));

    // Nếu không có vùng giao nhau, không có va chạm
    if (left >= right || top >= bottom)
        return false;

    // Duyệt qua từng pixel trong vùng giao nhau để kiểm tra
    for (int y = top; y < bottom; y++)
    {
        for (int x = left; x < right; x++)
        {
            // Tính vị trí tương đối của pixel trong sprite A
            int axRel = x - (int)ax;
            int ayRel = y - (int)ay;
            // Tính vị trí tương đối của pixel trong sprite B
            int bxRel = x - (int)bx;
            int byRel = y - (int)by;
            // Lấy trạng thái pixel của sprite A và sprite B
            bool pixelA = getBitmapPixel(bitmapA, aw, ah, axRel, ayRel);
            bool pixelB = getBitmapPixel(bitmapB, bw, bh, bxRel, byRel);
            // Nếu cả hai pixel đều bật, xảy ra va chạm
            if (pixelA && pixelB)
                return true;
        }
    }
    // Nếu không có pixel nào bật cùng lúc, không có va chạm
    return false;
}

// Hàm getPlayerSprite: Lựa chọn sprite của nhân vật dựa trên trạng thái nút bấm và vị trí của nhân vật
Sprite getPlayerSprite()
{
    Sprite s;
    // Nếu nút cúi được nhấn (LOW), sử dụng sprite cúi
    if (digitalRead(BUTTON_DUCK) == LOW)
    {
        s.bitmap = ninja_duck;
        s.width = 16;
        s.height = 16;
    }
    // Nếu nhân vật đang ở trạng thái nhảy (không chạm mặt đất)
    else if (player.y < GROUND_Y)
    {
        s.bitmap = ninja_jump;
        s.width = 16;
        s.height = 16;
    }
    // Ngược lại, nếu nhân vật đang chạy trên mặt đất, thay đổi frame để tạo hiệu ứng chạy
    else
    {
        s.bitmap = (player.frame ? ninja_run1 : ninja_run2);
        s.width = 16;
        s.height = 16;
    }
    return s;
}

// Hàm getObstacleSprite: Lấy sprite của chướng ngại vật dựa trên loại của nó
Sprite getObstacleSprite(GameObject &ob)
{
    Sprite s;
    switch (ob.type)
    {
    case TREE_SMALL:
        s.bitmap = tree_small;
        s.width = 8;
        s.height = 16;
        break;
    case TREE_LARGE:
        s.bitmap = tree_large;
        s.width = 12;
        s.height = 16;
        break;
    case KUNAI_LOW:
    case KUNAI_HIGH:
        s.bitmap = kunai;
        s.width = 16;
        s.height = 8;
        break;
    default:
        s.bitmap = NULL;
        s.width = 0;
        s.height = 0;
    }
    return s;
}

// Hàm setup: Thiết lập cấu hình ban đầu của Arduino
void setup()
{
    Serial.begin(115200);               // Khởi tạo giao tiếp Serial với tốc độ 115200 baud
    pinMode(BUTTON_JUMP, INPUT_PULLUP); // Cấu hình chân nút nhảy với điện trở kéo lên
    pinMode(BUTTON_DUCK, INPUT_PULLUP); // Cấu hình chân nút cúi với điện trở kéo lên
    pinMode(BUZZER_PIN, OUTPUT);        // Cấu hình chân buzzer là OUTPUT

    // Khởi tạo màn hình OLED; nếu không thành công, in ra thông báo lỗi và dừng chương trình
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;)
            ;
    }

    EEPROM.begin(512);                          // Khởi tạo EEPROM với dung lượng 512 byte
    highScore = EEPROM.read(EEPROM_HIGH_SCORE); // Đọc điểm cao từ EEPROM

    display.clearDisplay();      // Xóa màn hình OLED
    display.setTextSize(1);      // Đặt kích thước chữ mặc định
    display.setTextColor(WHITE); // Đặt màu chữ là trắng

    initializeGame();   // Khởi tạo các thông số của trò chơi
    showSplashScreen(); // Hiển thị màn hình chào (Splash Screen)
}

// Hàm loop: Vòng lặp chính chạy liên tục sau setup()
void loop()
{
    unsigned long currentTime = millis(); // Lấy thời gian hiện tại (ms)

    // Kiểm tra nếu đủ thời gian cho frame mới (16ms ~ 60FPS)
    if (currentTime - lastFrameTime >= 16)
    {
        // Xử lý theo từng trạng thái của trò chơi
        switch (gameState)
        {
        case MENU:
            handleMenu(); // Xử lý màn hình menu
            break;
        case PLAYING:
            handleGameplay(); // Xử lý quá trình chơi game
            break;
        case GAME_OVER:
            handleGameOver(); // Xử lý khi trò chơi kết thúc
            break;
        }

        lastFrameTime = currentTime; // Cập nhật thời gian frame cuối cùng
        frameCount++;                // Tăng số lượng frame đã chạy
    }
}

// Hàm handleGameplay: Xử lý các logic trong khi trò chơi đang được chơi
void handleGameplay()
{
    handleInput();         // Xử lý đầu vào từ người dùng (nút nhảy, nút cúi)
    updateGame();          // Cập nhật logic trò chơi (vị trí, vận tốc,…)
    handleCollisions();    // Kiểm tra va chạm giữa nhân vật và chướng ngại vật
    updateDayNightCycle(); // Cập nhật chu kỳ chuyển đổi giữa ban ngày và ban đêm
    drawGame();            // Vẽ lại toàn bộ trò chơi lên màn hình OLED
}

// Hàm updateGame: Cập nhật trạng thái, vị trí và tốc độ của nhân vật, chướng ngại vật và mây
void updateGame()
{
    // Nếu nhân vật đang nhảy hoặc rơi (không chạm mặt đất)
    if (player.y < GROUND_Y || player.velocity_y < 0)
    {
        // Nếu nhân vật đang nhảy và nút cúi được nhấn, tăng gia tốc rơi (nhằm hạ nhanh)
        if (player.y < GROUND_Y && digitalRead(BUTTON_DUCK) == LOW)
        {
            player.velocity_y += GRAVITY * 2;
        }
        else
        {
            // Ngược lại, tăng vận tốc theo trọng lực thông thường
            player.velocity_y += GRAVITY;
        }

        // Giới hạn vận tốc rơi không vượt quá MAX_FALL_SPEED
        if (player.velocity_y > MAX_FALL_SPEED)
        {
            player.velocity_y = MAX_FALL_SPEED;
        }

        player.y += player.velocity_y; // Cập nhật vị trí y của nhân vật

        // Nếu nhân vật rơi quá thấp, đưa trở lại mặt đất và đặt lại vận tốc
        if (player.y > GROUND_Y)
        {
            player.y = GROUND_Y;
            player.velocity_y = 0;
        }
    }

    // Nếu nhân vật đang chạy trên mặt đất, thay đổi frame hoạt ảnh định kỳ
    if (player.y >= GROUND_Y && frameCount % 6 == 0)
    {
        player.frame = !player.frame;
    }

    // Cập nhật vị trí của các chướng ngại vật
    for (int i = 0; i < 3; i++)
    {
        if (obstacles[i].active)
        { // Nếu chướng ngại vật đang hoạt động
            // Cập nhật vị trí x của chướng ngại vật dựa vào vận tốc và tốc độ trò chơi
            obstacles[i].x += obstacles[i].velocity_x * gameSpeed;

            // Nếu chướng ngại vật là loại kunai, cập nhật frame cho hoạt ảnh (nếu có)
            if (obstacles[i].type == KUNAI_LOW || obstacles[i].type == KUNAI_HIGH)
            {
                if (frameCount % 10 == 0)
                {
                    obstacles[i].frame = !obstacles[i].frame;
                }
            }

            // Nếu chướng ngại vật đã đi ra khỏi màn hình, đặt lại vị trí và tăng điểm số
            if (obstacles[i].x < -obstacles[i].width)
            {
                resetObstacle(i); // Đặt lại thông số chướng ngại vật
                score++;          // Tăng điểm

                // Mỗi khi đạt được số điểm chia hết cho 15, tăng tốc độ của trò chơi
                if (score % 15 == 0)
                {
                    gameSpeed += 0.1;
                }
            }
        }
    }

    // Cập nhật vị trí của các mây
    for (int i = 0; i < 2; i++)
    {
        if (clouds[i].active)
        {
            clouds[i].x += clouds[i].velocity_x; // Di chuyển mây theo hướng x
            // Nếu mây đi ra khỏi màn hình bên trái, đưa mây trở lại bên phải với vị trí y ngẫu nhiên
            if (clouds[i].x < -clouds[i].width)
            {
                clouds[i].x = SCREEN_WIDTH;
                clouds[i].y = random(0, SCREEN_HEIGHT / 2);
            }
        }
    }
}

// Hàm handleCollisions: Kiểm tra va chạm giữa nhân vật và các chướng ngại vật
void handleCollisions()
{
    for (int i = 0; i < 3; i++)
    {
        if (obstacles[i].active)
        { // Nếu chướng ngại vật đang hoạt động
            // Lấy sprite của nhân vật và của chướng ngại vật để kiểm tra va chạm chính xác theo pixel
            Sprite playerSprite = getPlayerSprite();
            Sprite obstacleSprite = getObstacleSprite(obstacles[i]);
            if (pixelCollision(player.x, player.y, playerSprite.width, playerSprite.height, playerSprite.bitmap,
                               obstacles[i].x, obstacles[i].y, obstacleSprite.width, obstacleSprite.height, obstacleSprite.bitmap))
            {
                gameOver(); // Nếu va chạm xảy ra, chuyển sang trạng thái GAME_OVER
                return;
            }
        }
    }
}

// Hàm checkCollision: Kiểm tra va chạm dựa trên bounding box với biên trừ (không dùng trong hàm chính)
// Hàm này sử dụng margin để tránh việc phát hiện va chạm giả do sprite có khoảng trống
bool checkCollision(GameObject &a, GameObject &b)
{
    const int marginPlayer = 2;
    const int marginObstacle = 2;

    return (a.x + marginPlayer < b.x + b.width - marginObstacle &&
            a.x + a.width - marginPlayer > b.x + marginObstacle &&
            a.y + marginPlayer < b.y + b.height - marginObstacle &&
            a.y + a.height - marginPlayer > b.y + marginObstacle);
}

// Hàm drawGame: Vẽ toàn bộ các thành phần của trò chơi lên màn hình OLED
void drawGame()
{
    display.clearDisplay(); // Xóa màn hình

    // Nếu đang ở trạng thái ban đêm, vẽ các "ngôi sao" (pixel) lên màn hình
    if (isNight)
    {
        for (int i = 0; i < 5; i++)
        {
            int x = (SCREEN_WIDTH - 1) - ((frameCount * 2 + i * 30) % SCREEN_WIDTH);
            display.drawPixel(x, i * 10 + 5, WHITE);
        }
    }

    // Vẽ các mây lên màn hình
    for (int i = 0; i < 2; i++)
    {
        if (clouds[i].active)
        {
            display.drawBitmap(clouds[i].x, clouds[i].y, cloud, 16, 8, WHITE);
        }
    }

    // Vẽ đường mặt đất (một đường thẳng từ trái qua phải)
    display.drawLine(0, SCREEN_HEIGHT - 1, SCREEN_WIDTH, SCREEN_HEIGHT - 1, WHITE);

    // Vẽ nhân vật: Lựa chọn sprite dựa trên trạng thái nút bấm và vị trí của nhân vật
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
        display.drawBitmap(player.x, player.y, player.frame ? ninja_run1 : ninja_run2, 16, 16, WHITE);
    }

    // Vẽ các chướng ngại vật lên màn hình theo loại của chúng
    for (int i = 0; i < 3; i++)
    {
        if (obstacles[i].active)
        {
            switch (obstacles[i].type)
            {
            case TREE_SMALL:
                display.drawBitmap(obstacles[i].x, obstacles[i].y, tree_small, 8, 16, WHITE);
                break;
            case TREE_LARGE:
                display.drawBitmap(obstacles[i].x, obstacles[i].y, tree_large, 12, 16, WHITE);
                break;
            case KUNAI_LOW:
            case KUNAI_HIGH:
                display.drawBitmap(obstacles[i].x, obstacles[i].y, kunai, 16, 8, WHITE);
                break;
            }
        }
    }

    // Hiển thị điểm số lên màn hình (ở góc trên bên phải)
    display.setCursor(SCREEN_WIDTH - 40, 5);
    display.print(score);

    display.display(); // Cập nhật toàn bộ các thay đổi lên màn hình OLED
}

// Hàm handleInput: Xử lý đầu vào từ các nút nhấn của người dùng
void handleInput()
{
    // Sử dụng biến tĩnh để lưu trạng thái nhấn của nút nhảy từ lần trước
    static bool canJump = true;
    static bool jumpButtonPrevious = true;
    bool jumpButtonCurrent = digitalRead(BUTTON_JUMP); // Đọc trạng thái nút nhảy hiện tại

    // Nếu nhân vật đang trên mặt đất, cho phép nhảy lại
    if (player.y >= GROUND_Y)
    {
        canJump = true;
    }

    // Nếu nút nhảy được nhấn (chuyển từ HIGH sang LOW) và được phép nhảy
    if (jumpButtonCurrent == LOW && jumpButtonPrevious == HIGH && canJump)
    {
        player.velocity_y = JUMP_FORCE; // Áp dụng lực nhảy cho nhân vật
        canJump = false;                // Vô hiệu hóa nhảy liên tục khi nút giữ
    }

    // Cập nhật trạng thái nút nhảy để sử dụng cho lần kiểm tra tiếp theo
    jumpButtonPrevious = jumpButtonCurrent;
}

// Hàm resetObstacle: Đặt lại thông số của chướng ngại vật sau khi nó đi ra khỏi màn hình
void resetObstacle(int index)
{
    // Đặt vị trí x của chướng ngại vật ra bên phải màn hình với khoảng cách ngẫu nhiên
    obstacles[index].x = SCREEN_WIDTH + random(115, 150);

    // Chọn ngẫu nhiên loại chướng ngại vật (0: TREE_SMALL, 1: TREE_LARGE, 2: KUNAI_LOW, 3: KUNAI_HIGH)
    int type = random(4);

    switch (type)
    {
    case TREE_SMALL:
        obstacles[index].width = 8;
        obstacles[index].height = 16;
        obstacles[index].y = SCREEN_HEIGHT - 16; // Đặt chướng ngại vật ngay trên mặt đất
        break;
    case TREE_LARGE:
        obstacles[index].width = 12;
        obstacles[index].height = 16;
        obstacles[index].y = SCREEN_HEIGHT - 16;
        break;
    case KUNAI_LOW:
        obstacles[index].width = 16;
        obstacles[index].height = 8;
        obstacles[index].y = SCREEN_HEIGHT - 15; // Vị trí thấp cho kunai
        break;
    case KUNAI_HIGH:
        obstacles[index].width = 16;
        obstacles[index].height = 8;
        obstacles[index].y = SCREEN_HEIGHT - 19; // Vị trí cao hơn cho kunai
        break;
    }

    obstacles[index].type = type;     // Gán loại chướng ngại vật vừa chọn
    obstacles[index].active = true;   // Đánh dấu chướng ngại vật là đang hoạt động
    obstacles[index].velocity_x = -3; // Đặt vận tốc di chuyển sang trái
    obstacles[index].frame = 0;       // Khởi tạo frame hoạt ảnh cho chướng ngại vật
}

// Hàm gameOver: Xử lý khi game kết thúc (va chạm xảy ra)
void gameOver()
{
    gameState = GAME_OVER; // Chuyển trạng thái trò chơi sang GAME_OVER

    // Nếu điểm hiện tại cao hơn điểm cao nhất, cập nhật điểm cao và lưu vào EEPROM
    if (score > highScore)
    {
        highScore = score;
        EEPROM.write(EEPROM_HIGH_SCORE, highScore);
        EEPROM.commit(); // Cam kết thay đổi dữ liệu vào EEPROM
    }
}

// Hàm showSplashScreen: Hiển thị màn hình chào khi khởi động trò chơi
void showSplashScreen()
{
    display.clearDisplay();     // Xóa màn hình
    display.setTextSize(2);     // Đặt kích thước chữ lớn cho tiêu đề
    display.setCursor(10, 5);   // Đặt vị trí con trỏ cho tiêu đề
    display.print("NINJA RUN"); // In tiêu đề trò chơi
    display.setTextSize(1);     // Đặt kích thước chữ nhỏ cho hướng dẫn
    display.setCursor(8, 40);   // Đặt vị trí cho hướng dẫn bắt đầu
    display.print("Press JUMP to start");
    display.display(); // Cập nhật hiển thị lên màn hình

    // Chờ cho đến khi nút nhảy được nhấn (bắt đầu trò chơi)
    while (digitalRead(BUTTON_JUMP) == HIGH)
    {
        delay(10); // Delay ngắn để tránh tiêu thụ CPU quá mức
    }

    gameState = PLAYING; // Chuyển trạng thái trò chơi sang PLAYING
}

// Hàm handleMenu: Xử lý màn hình menu chính của trò chơi
void handleMenu()
{
    display.clearDisplay();     // Xóa màn hình
    display.setTextSize(2);     // Đặt kích thước chữ lớn cho tiêu đề
    display.setCursor(8, 5);    // Đặt vị trí cho tiêu đề
    display.print("NINJA RUN"); // In tiêu đề trò chơi

    display.setCursor(20, 45); // Đặt vị trí cho hướng dẫn
    display.print("Press JUMP to start");
    display.display(); // Cập nhật hiển thị lên màn hình

    // Nếu nút nhảy được nhấn, bắt đầu trò chơi mới
    if (digitalRead(BUTTON_JUMP) == LOW)
    {
        delay(100);          // Delay ngắn để tránh rung nút
        initializeGame();    // Khởi tạo lại các thông số trò chơi
        gameState = PLAYING; // Chuyển trạng thái sang PLAYING
    }
}

// Hàm handleGameOver: Xử lý màn hình hiển thị khi trò chơi kết thúc
void handleGameOver()
{
    display.clearDisplay();     // Xóa màn hình
    display.setTextSize(2);     // Đặt kích thước chữ lớn cho tiêu đề "GAME OVER"
    display.setCursor(11, 0);   // Đặt vị trí cho tiêu đề
    display.print("GAME OVER"); // In thông báo kết thúc trò chơi

    display.setTextSize(1);   // Đặt kích thước chữ nhỏ cho thông tin điểm số
    display.setCursor(0, 26); // Đặt vị trí cho hiển thị điểm
    display.print("Score: ");
    display.print(score); // In ra điểm số của người chơi

    display.setCursor(0, 45); // Đặt vị trí cho hướng dẫn restart
    display.print("Press JUMP to restart");
    display.display(); // Cập nhật hiển thị lên màn hình

    // Nếu nút nhảy được nhấn, khởi động lại trò chơi
    if (digitalRead(BUTTON_JUMP) == LOW)
    {
        delay(200);          // Delay nhỏ trước khi khởi động lại
        initializeGame();    // Khởi tạo lại trò chơi
        gameState = PLAYING; // Chuyển trạng thái sang PLAYING
    }
}

// Hàm initializeGame: Khởi tạo lại các thông số, đối tượng trong trò chơi
void initializeGame()
{
    // Khởi tạo lại thông số của nhân vật chính
    player.x = 20;
    player.y = GROUND_Y;
    player.width = 16;
    player.height = 16;
    player.velocity_x = 0;
    player.velocity_y = 0;
    player.active = true;
    player.frame = 0;

    // Khởi tạo lại các chướng ngại vật
    for (int i = 0; i < 3; i++)
    {
        obstacles[i].active = false;             // Vô hiệu hoá chướng ngại vật trước khi thiết lập lại
        resetObstacle(i);                        // Đặt lại thông số của chướng ngại vật
        obstacles[i].x = SCREEN_WIDTH + i * 100; // Đặt vị trí ban đầu, cách nhau 100 pixel
    }

    // Khởi tạo lại các mây
    for (int i = 0; i < 2; i++)
    {
        clouds[i].x = SCREEN_WIDTH + i * 80;        // Đặt vị trí ban đầu cho mây, cách nhau 80 pixel
        clouds[i].y = random(0, SCREEN_HEIGHT / 2); // Đặt vị trí y ngẫu nhiên trong nửa trên màn hình
        clouds[i].width = 12;                       // Đặt chiều rộng cho mây
        clouds[i].height = 6;                       // Đặt chiều cao cho mây
        clouds[i].velocity_x = -1;                  // Vận tốc của mây (di chuyển sang trái)
        clouds[i].active = true;                    // Kích hoạt đối tượng mây
    }

    score = 0;                // Đặt lại điểm số ban đầu
    gameSpeed = 1.0;          // Đặt lại tốc độ của trò chơi
    frameCount = 0;           // Đặt lại số frame đã chạy
    isNight = false;          // Đặt trạng thái ban ngày
    dayNightTimer = millis(); // Lấy thời gian hiện tại cho bộ đếm chuyển chu kỳ ngày/đêm
}

// Hàm updateDayNightCycle: Cập nhật chu kỳ chuyển đổi giữa ban ngày và ban đêm
void updateDayNightCycle()
{
    // Nếu đã trôi qua đủ thời gian so với DAY_NIGHT_DURATION, đảo trạng thái ban ngày/ban đêm
    if (millis() - dayNightTimer >= DAY_NIGHT_DURATION)
    {
        isNight = !isNight;       // Đảo trạng thái: nếu đang ban ngày chuyển sang ban đêm và ngược lại
        dayNightTimer = millis(); // Cập nhật lại thời gian bắt đầu của chu kỳ mới
    }
}
