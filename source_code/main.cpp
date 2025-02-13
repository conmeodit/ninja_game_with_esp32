/*
 * Tên dự án: T-rex-duino
 * Mô tả: Trò chơi T-rex được chuyển từ trình duyệt Chrome sang Arduino
 * Trang dự án: https://github.com/AlexIII/t-rex-duino
 * Tác giả: github.com/AlexIII
 * Email: endoftheworld@bk.ru
 * Giấy phép: MIT
 *
 * -------Phần cứng------
 * Bo mạch: Arduino Uno / Nano / Pro / pro mini
 * Màn hình: OLED SSD1309 SPI 128x64 HOẶC SH1106/SSD1306 I2C 128x64 (130x64) (132x64)
 */

/* Cài đặt kết nối phần cứng */
// -- Các nút nhấn --
#define JUMP_BUTTON 6 // Nút nhảy được kết nối với chân số 6
#define DUCK_BUTTON 5 // Nút cúi được kết nối với chân số 5

// -- Lựa chọn màn hình (bỏ comment MỘT trong các tùy chọn) --
#define LCD_SSD1309 // Sử dụng màn hình SSD1309
// #define LCD_SH1106       // Nếu thấy có đường thẳng bất thường ở cạnh trái của màn hình, hãy dùng LCD_SSD1306
// #define LCD_SSD1306      // Nếu thấy có đường thẳng bất thường ở cạnh phải của màn hình, hãy dùng LCD_SH1106

// -- Kết nối màn hình cho SSD1309 --
#define LCD_SSD1309_CS 2    // Chân Chip Select của SSD1309: chân 2
#define LCD_SSD1309_DC 3    // Chân Data/Command của SSD1309: chân 3
#define LCD_SSD1309_RESET 4 // Chân Reset của SSD1309: chân 4
// Ghi chú:
// LCD_SSD1309_SDA -> chân 11 (SPI SCK)
// LCD_SSD1309_SCL -> chân 13 (SPI MOSI)

// -- Kết nối màn hình cho SH1106/SSD1306 --
// LCD_SH1106_SDA -> chân A4 (I2C SDA)
// LCD_SH1106_SCL -> chân A5 (I2C SCL)

/* Cài đặt khác */
// #define AUTO_PLAY         // Bỏ comment để kích hoạt chế độ chơi tự động
// #define RESET_HI_SCORE    // Bỏ comment để reset điểm cao (nếu flash thiết bị, reset rồi comment lại và flash lại)
// #define PRINT_DEBUG_INFO  // Bỏ comment để in thông tin gỡ lỗi

/* Cài đặt cân bằng trò chơi */
// PLAYER_SAFE_ZONE_WIDTH: khoảng cách an toàn tối thiểu giữa các chướng ngại vật (đơn vị pixel)
#define PLAYER_SAFE_ZONE_WIDTH 32
// CACTI_RESPAWN_RATE: tần suất xuất hiện của cây xương rồng, giá trị càng thấp thì càng xuất hiện nhiều (max 255)
#define CACTI_RESPAWN_RATE 50
// GROUND_CACTI_SCROLL_SPEED: tốc độ di chuyển của cáctus trên mặt đất (pixels mỗi chu kỳ game)
#define GROUND_CACTI_SCROLL_SPEED 3
// PTERODACTY_SPEED: tốc độ của Pterodactyl (pixels mỗi chu kỳ game)
#define PTERODACTY_SPEED 5
// PTERODACTY_RESPAWN_RATE: tần suất xuất hiện của Pterodactyl (giá trị càng thấp thì càng xuất hiện nhiều, max 255)
#define PTERODACTY_RESPAWN_RATE 255
// INCREASE_FPS_EVERY_N_SCORE_POINTS: tăng FPS sau mỗi khi đạt được số điểm nhất định (nên là lũy thừa của 2)
#define INCREASE_FPS_EVERY_N_SCORE_POINTS 256
// LIVES_START: số mạng sống ban đầu của người chơi
#define LIVES_START 3
// LIVES_MAX: số mạng sống tối đa người chơi có thể đạt được
#define LIVES_MAX 5
// SPAWN_NEW_LIVE_MIN_CYCLES: số chu kỳ tối thiểu trước khi xuất hiện mạng sống mới
#define SPAWN_NEW_LIVE_MIN_CYCLES 800
// DAY_NIGHT_SWITCH_CYCLES: số chu kỳ sau đó chuyển đổi giữa ngày và đêm (nên là lũy thừa của 2)
#define DAY_NIGHT_SWITCH_CYCLES 1024
// TARGET_FPS_START: FPS ban đầu của trò chơi
#define TARGET_FPS_START 23
// TARGET_FPS_MAX: FPS tối đa của trò chơi, FPS sẽ tăng dần để làm game khó hơn
#define TARGET_FPS_MAX 48

/* Cài đặt màn hình */
#define LCD_HEIGHT 64U // Chiều cao màn hình: 64 pixel
#define LCD_WIDTH 128U // Chiều rộng màn hình: 128 pixel

/* Cài đặt vẽ đồ họa (Render Settings) */
#ifdef LCD_SSD1309
// #define VIRTUAL_HEIGHT_BUFFER_ROWS_BY_8_PIXELS 1
//  Nếu sử dụng SSD1309, ta dùng buffer ảo với 16 cột
#define VIRTUAL_WIDTH_BUFFER_COLS 16
#else
// Với SH1106/SSD1306, không hỗ trợ VIRTUAL_WIDTH_BUFFER nên dùng VIRTUAL_HEIGHT_BUFFER_ROWS_BY_8_PIXELS
#define VIRTUAL_HEIGHT_BUFFER_ROWS_BY_8_PIXELS 4
#endif

/* Thư viện include */
#include <EEPROM.h>
#include "array.h"
#include "TrexPlayer.h"
#include "Ground.h"
#include "Cactus.h"
#include "Pterodactyl.h"
#include "HeartLive.h"

/* Định nghĩa và biến toàn cục */
#define EEPROM_HI_SCORE 16                         // Vị trí lưu điểm cao trong EEPROM (2 bytes)
#define LCD_BYTE_SZIE (LCD_WIDTH * LCD_HEIGHT / 8) // Kích thước mảng byte của màn hình

#ifdef VIRTUAL_WIDTH_BUFFER_COLS
#define LCD_IF_VIRTUAL_WIDTH(TRUE_COND, FALSE_COND) TRUE_COND
#define LCD_PART_BUFF_WIDTH VIRTUAL_WIDTH_BUFFER_COLS
#define LCD_PART_BUFF_HEIGHT LCD_HEIGHT
#else
#define LCD_IF_VIRTUAL_WIDTH(TRUE_COND, FALSE_COND) FALSE_COND
#ifdef VIRTUAL_HEIGHT_BUFFER_ROWS_BY_8_PIXELS
#define LCD_PART_BUFF_WIDTH LCD_WIDTH
#define LCD_PART_BUFF_HEIGHT (VIRTUAL_HEIGHT_BUFFER_ROWS_BY_8_PIXELS * 8)
#else
#define LCD_PART_BUFF_WIDTH LCD_WIDTH
#define LCD_PART_BUFF_HEIGHT LCD_HEIGHT
#endif
#endif
#define LCD_PART_BUFF_SZ ((LCD_PART_BUFF_HEIGHT / 8) * LCD_PART_BUFF_WIDTH)

#ifdef LCD_SSD1309
// Nếu sử dụng màn hình SSD1309, bao gồm thư viện SPI và khởi tạo đối tượng lcd với giao diện SPI
#include <SPI.h>
#include "SSD1309.h"
static SSD1309<SPIClass> lcd(SPI, LCD_SSD1309_CS, LCD_SSD1309_DC, LCD_SSD1309_RESET, LCD_BYTE_SZIE);
#else
// Nếu sử dụng màn hình SH1106/SSD1306, sử dụng giao tiếp I2C
#include "I2C.h"
#include "SH1106.h"
I2C i2c;
SH1106<I2C> lcd(i2c, LCD_BYTE_SZIE);
#endif

// Biến lưu điểm cao (hi score) toàn cục, ban đầu là 0
static uint16_t hiScore = 0;
static bool firstStart = true;

/* Các hàm phụ trợ (Misc Functions) */

// Hàm kiểm tra xem nút nhảy có được nhấn không (do sử dụng INPUT_PULLUP nên khi nhấn sẽ trả về LOW)
bool isPressedJump()
{
    return !digitalRead(JUMP_BUTTON);
}

// Hàm kiểm tra xem nút cúi có được nhấn không
bool isPressedDuck()
{
    return !digitalRead(DUCK_BUTTON);
}

// Hàm sinh số ngẫu nhiên kiểu uint8_t, sử dụng các phép dịch bit và đọc analog để tạo độ ngẫu nhiên
uint8_t randByte()
{
    static uint16_t c = 0xA7E2;
    c = (c << 1) | (c >> 15);
    c = (c << 1) | (c >> 15);
    c = (c << 1) | (c >> 15);
    c = analogRead(A2) ^ analogRead(A3) ^ analogRead(A4) ^ analogRead(A5) ^ analogRead(A6) ^ analogRead(A7) ^ c;
    return c;
}

/* Các hàm chính của chương trình */

// Hàm vẽ số lên màn hình sử dụng đối tượng BitCanvas
// Tham số point xác định vị trí bắt đầu, number là số cần hiển thị
void renderNumber(BitCanvas &canvas, Point2Di8 point, const uint16_t number)
{
    uint16_t base = 10000; // Khởi tạo cơ số (cho 5 chữ số)
    while (base)
    {
        // Lấy chữ số cần vẽ ở vị trí hiện tại
        const uint8_t digit = (number / base) % 10;
        // Vẽ sprite của chữ số lên canvas tại vị trí point
        canvas.render(numbers.getSprite(digit, point));
        base /= 10;                        // Giảm cơ số để chuyển sang chữ số tiếp theo
        point.x += numbers.getWidth() + 1; // Dịch chuyển sang phải một khoảng bằng chiều rộng sprite + 1 pixel
    }
}

// Hàm gameLoop: vòng lặp chính của trò chơi
void gameLoop(uint16_t &hiScore)
{
    // Tạo bộ đệm cho một phần của màn hình với kích thước đã tính trước
    uint8_t lcdBuff[LCD_PART_BUFF_SZ];
    VirtualBitCanvas bitCanvas(
        LCD_IF_VIRTUAL_WIDTH(VirtualBitCanvas::VIRTUAL_WIDTH, VirtualBitCanvas::VIRTUAL_HEIGHT),
        lcdBuff,
        LCD_PART_BUFF_HEIGHT,
        LCD_PART_BUFF_WIDTH,
        LCD_IF_VIRTUAL_WIDTH(LCD_WIDTH, LCD_HEIGHT) // Kích thước ảo theo chiều đã chọn (rộng hoặc cao)
    );

    // Đối tượng giữ thông tin điều phối spawn cho các đối tượng chướng ngại vật
    SpawnHold spawnHolder;

    // Khởi tạo các sprite động trong game
    TrexPlayer trex;                       // Nhân vật T-rex
    Ground ground1(-1);                    // Đối tượng mặt đất, vị trí khởi tạo -1
    Ground ground2(63);                    // Đối tượng mặt đất, vị trí khởi tạo 63
    Ground ground3(127);                   // Đối tượng mặt đất, vị trí khởi tạo 127
    Cactus cactus1(spawnHolder);           // Cactus đầu tiên, sử dụng spawnHolder để quản lý thời gian xuất hiện
    Cactus cactus2(spawnHolder);           // Cactus thứ hai
    Pterodactyl pterodactyl1(spawnHolder); // Đối tượng Pterodactyl (loài khủng long bay)
    HeartLive heartLive;                   // Đối tượng heart/live (mang tính năng tăng mạng sống)

    // Mảng chứa tất cả các sprite động (sử dụng template array)
    const array<SpriteAnimated *, 8> sprites{{&ground1, &ground2, &ground3, &cactus1, &cactus2, &pterodactyl1, &heartLive, &trex}};
    // Mảng chứa các đối tượng kẻ địch để kiểm tra va chạm
    const array<SpriteAnimated *, 3> enemies{{&cactus1, &cactus2, &pterodactyl1}};

    // Khởi tạo các sprite tĩnh phục vụ cho giao diện game
    const Sprite gameOverSprite(&game_overver_bm, {15, 12});    // Sprite hiển thị "Game Over"
    const Sprite restartIconSprite(&restart_icon_bm, {55, 25}); // Icon restart (chơi lại)
    const Sprite hiSprite(&hi_score, {44, 0});                  // Icon "Hi Score"
    Sprite heartsSprite(&hearts_5x_bm, {95, 8});                // Sprite hiển thị số mạng sống (hearts)

    // Các biến điều khiển game
    uint32_t prvT = 0;                    // Thời gian của chu kỳ trước (để điều chỉnh tốc độ khung hình)
    bool gameOver = false;                // Cờ báo hiệu game kết thúc hay chưa
    uint16_t score = 0;                   // Điểm số hiện tại
    uint8_t targetFPS = TARGET_FPS_START; // FPS mục tiêu ban đầu
    uint8_t lives = LIVES_START;          // Số mạng sống ban đầu
    bool night = false;                   // Cờ cho chế độ ban đêm (false: ban ngày, true: ban đêm)
    lcd.setInverse(night);                // Cài đặt chế độ hiển thị đảo màu (inverse) dựa trên trạng thái day/night

    // Vòng lặp chính của trò chơi
    while (1)
    {
        // Vòng lặp render: vẽ giao diện lên màn hình qua canvas
        while (1)
        {
            // Vẽ các thông tin giao diện lên canvas
            bitCanvas.render(hiSprite);                // Vẽ icon "Hi Score"
            renderNumber(bitCanvas, {60, 0}, hiScore); // Vẽ điểm cao
            renderNumber(bitCanvas, {95, 0}, score);   // Vẽ điểm số hiện tại
            bitCanvas.render(heartsSprite);            // Vẽ số mạng sống (hearts)

            // Vẽ các đối tượng game (sprite động)
            for (uint8_t i = 0; i < sprites.size(); ++i)
                bitCanvas.render(*sprites[i]);

            // Nếu game kết thúc, vẽ thêm giao diện "Game Over" và biểu tượng restart
            if (gameOver)
            {
                bitCanvas.render(gameOverSprite);
                bitCanvas.render(restartIconSprite);
            }

            // Cập nhật nội dung buffer lên màn hình LCD
            lcd.fillScreen(lcdBuff, LCD_PART_BUFF_SZ, LCD_IF_VIRTUAL_WIDTH(LCD_PART_BUFF_WIDTH, 0));

            // Nếu đã vẽ xong phần hiện tại, thoát vòng lặp render để xử lý logic
            if (bitCanvas.nextPart())
                break;
        }

        // Nếu game kết thúc, cập nhật điểm cao nếu cần và thoát khỏi gameLoop
        if (gameOver)
        {
            if (score > hiScore)
                hiScore = score;
            return;
        }

        // Kiểm tra va chạm giữa T-rex và các kẻ địch
        if (!trex.isBlinking() && CollisionDetector::check(trex, enemies.data, enemies.size()))
        {
            if (lives)
            {
                trex.blink(); // T-rex nhấp nháy biểu thị va chạm (vẫn còn mạng sống)
                --lives;      // Giảm số mạng sống
            }
            else
            {
                trex.die();      // T-rex chết
                gameOver = true; // Đánh dấu game kết thúc
                continue;        // Bỏ qua phần xử lý còn lại của vòng lặp
            }
        }
        // Kiểm tra va chạm giữa T-rex và đối tượng heartLive để tăng mạng sống
        if (lives < LIVES_MAX && CollisionDetector::check(trex, heartLive))
        {
            ++lives;         // Tăng số mạng sống
            heartLive.eat(); // Đánh dấu đối tượng heartLive đã bị "ăn"
        }

#ifndef AUTO_PLAY
        // Nếu không ở chế độ chơi tự động, xử lý điều khiển từ người chơi:
        if (isPressedJump())
            trex.jump();            // Nếu nút nhảy được nhấn, T-rex sẽ nhảy
        trex.duck(isPressedDuck()); // Nếu nút cúi được nhấn, T-rex sẽ cúi
#else
        // Chế độ chơi tự động (AUTO_PLAY)
        const int8_t trexXright = trex.bitmap->width + trex.position.x; // Tính vị trí bên phải của T-rex
        // Tự động nhảy nếu có cactus gần T-rex
        if (
            (cactus1.position.x <= trexXright + 5 && cactus1.position.x > trexXright) ||
            (cactus2.position.x <= trexXright + 5 && cactus2.position.x > trexXright) ||
            (pterodactyl1.position.y > 30 && pterodactyl1.position.x <= trexXright + 5 && pterodactyl1.position.x > trexXright))
            trex.jump();
        // Tự động cúi nếu có Pterodactyl bay thấp
        trex.duck(
            (pterodactyl1.position.y <= 30 && pterodactyl1.position.y > 20 && pterodactyl1.position.x <= trexXright + 15 && pterodactyl1.position.x > trex.position.x));
#endif

        // Cập nhật logic và animation cho các đối tượng game
        for (uint8_t i = 0; i < sprites.size(); ++i)
            sprites[i]->step();
        // Tăng điểm số (score) nếu chưa đạt giới hạn
        if (score < 0xFFFE)
            ++score;
        // Tăng FPS dần dần khi đạt được một số điểm nhất định (tăng độ khó của game)
        if (!(score % INCREASE_FPS_EVERY_N_SCORE_POINTS) && targetFPS < TARGET_FPS_MAX)
            ++targetFPS;
        // Cập nhật sprite hiển thị số mạng sống theo số mạng còn lại
        heartsSprite.limitRenderWidthTo = 6 * lives + 1;
        // Chuyển đổi giữa chế độ ngày và đêm sau số chu kỳ nhất định
        if (!(score % DAY_NIGHT_SWITCH_CYCLES))
            lcd.setInverse(night = !night);

        // Tính thời gian cần cho mỗi khung hình (frame)
        const uint8_t frameTime = 1000 / targetFPS;
#ifdef PRINT_DEBUG_INFO
        // Nếu bật gỡ lỗi, in ra thông tin sử dụng CPU
        const uint32_t dt = millis() - prvT;
        uint32_t left = frameTime > dt ? frameTime - dt : 0;
        Serial.print("CPU: ");
        Serial.print(100 - 100 * left / frameTime);
        Serial.print("% ");
        Serial.println(dt);
#endif

        // Đồng bộ vòng lặp để duy trì FPS mục tiêu
        while (millis() - prvT < frameTime)
            ;
        prvT = millis(); // Cập nhật thời gian cho vòng lặp tiếp theo
    }
}

// Hàm hiển thị màn hình chào (Splash Screen)
void spalshScreen()
{
    // Cài đặt chế độ định địa chỉ của màn hình thành Horizontal
    lcd.setAddressingMode(lcd.HorizontalAddressingMode);
    uint8_t buff[32];
    // Duyệt qua toàn bộ dữ liệu bitmap của splash screen, đọc từ bộ nhớ flash (PROGMEM)
    for (uint8_t i = 0; i < LCD_BYTE_SZIE / sizeof(buff); ++i)
    {
        memcpy_P(buff, splash_screen_bitmap + 2 + uint16_t(i) * sizeof(buff), sizeof(buff));
        lcd.fillScreen(buff, sizeof(buff));
    }
    // Chờ khoảng thời gian nhất định (50 lần delay 100ms) hoặc cho đến khi nút nhảy được nhấn
    for (uint8_t i = 50; i && !isPressedJump(); --i)
        delay(100);
}

// Hàm setup: chạy một lần khi thiết bị khởi động
void setup()
{
    // Cài đặt chân nút nhấn với chế độ INPUT_PULLUP (nếu nhấn sẽ trả về LOW)
    pinMode(JUMP_BUTTON, INPUT_PULLUP);
    pinMode(DUCK_BUTTON, INPUT_PULLUP);
    Serial.begin(250000); // Khởi tạo giao tiếp Serial với tốc độ 250000 baud
    lcd.begin();          // Khởi tạo màn hình LCD
    spalshScreen();       // Hiển thị màn hình chào
    // Cài đặt chế độ định địa chỉ của màn hình dựa vào loại màn hình đang dùng
    lcd.setAddressingMode(LCD_IF_VIRTUAL_WIDTH(lcd.VerticalAddressingMode, lcd.HorizontalAddressingMode));
    // Seed cho hàm rand() sử dụng 2 lần hàm randByte() (sinh số ngẫu nhiên)
    srand((randByte() << 8) | randByte());

#ifdef RESET_HI_SCORE
    // Nếu cài đặt RESET_HI_SCORE được bật, reset điểm cao trong EEPROM
    EEPROM.put(EEPROM_HI_SCORE, hiScore);
#endif
    // Đọc điểm cao từ EEPROM
    EEPROM.get(EEPROM_HI_SCORE, hiScore);
    if (hiScore == 0xFFFF)
        hiScore = 0; // Nếu dữ liệu không hợp lệ, đặt điểm cao về 0
}

// Hàm loop: vòng lặp chính của Arduino, chạy liên tục
void loop()
{
    // Nếu đây là lần chạy đầu tiên hoặc nút nhảy được nhấn, bắt đầu trò chơi
    if (firstStart || isPressedJump())
    {
        firstStart = false;
        gameLoop(hiScore);                    // Gọi vòng lặp game
        EEPROM.put(EEPROM_HI_SCORE, hiScore); // Lưu điểm cao mới vào EEPROM
        // Chờ cho đến khi nút nhảy được nhả ra
        while (isPressedJump())
            delay(100);
        delay(500); // Đợi thêm 500ms trước khi cho phép chơi lại
    }
}
