#include <Carbon/Carbon.h>
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <map>
#include <mutex>
#include <sys/stat.h>
#include <thread>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <unistd.h>

class KeyMonitor {
private:
  CFMachPortRef eventTap = nullptr;
  CGEventFlags currentFlags = 0;
  bool isKeyDownProcessed = false;

  // 按键计数
  std::map<int, int> keyCounts;
  std::map<int, int> modifierCounts;
  std::string logDir;
  std::string currentFilename;
  std::mutex filenameMutex;

  // 多线程控制
  std::thread idleCheckThread;
  std::thread dateCheckThread;
  std::mutex dataMutex;
  std::atomic<bool> running{true};
  std::chrono::time_point<std::chrono::system_clock> lastKeyTime;

  // 修饰键映射
  const std::map<int, std::string> modifierKeys = {
      {54, "RCmd"},  {55, "Cmd"},    {56, "Shift"}, {57, "Caps"}, {58, "LAlt"},
      {59, "LCtrl"}, {60, "RShift"}, {61, "RAlt"},  {62, "RCtrl"}};

  // 特殊键映射
  const std::map<int, std::string> keyNames = {
      {36, "Enter"},  {48, "Tab"},   {49, "Space"}, {51, "Delete"},
      {53, "Esc"},    {76, "Enter"}, {96, "F5"},    {97, "F6"},
      {98, "F7"},     {99, "F3"},    {100, "F8"},   {101, "F9"},
      {103, "F11"},   {105, "F13"},  {107, "F14"},  {109, "F10"},
      {113, "F12"},   {115, "Home"}, {116, "PgUp"}, {117, "Del"},
      {119, "End"},   {121, "PgDn"}, {122, "F4"},   {123, "Left"},
      {124, "Right"}, {125, "Down"}, {126, "Up"},   {114, "Help"},
      {118, "F2"},    {120, "F1"}};

  // 获取UTC+8时间
  std::tm getUTCP8Time() {
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm utc8_tm = *std::localtime(&now_time); // 获取本地时间
    // 如果是UTC时间，需要加上8小时
    // std::tm utc8_tm = *std::gmtime(&now_time);
    // utc8_tm.tm_hour += 8;
    // mktime(&utc8_tm);
    return utc8_tm;
  }

  // 生成文件名
  std::string generateFilename() {
    std::tm utc8_tm = getUTCP8Time();
    std::ostringstream oss;
    oss << logDir << "/" 
        << (utc8_tm.tm_year + 1900) << "-"
        << std::setw(2) << std::setfill('0') << (utc8_tm.tm_mon + 1) << "-"
        << std::setw(2) << std::setfill('0') << utc8_tm.tm_mday << ".log";
    return oss.str();
  }

  // 检查并创建目录
  void ensureDirectoryExists(const std::string& path) {
    if (access(path.c_str(), F_OK) == -1) {
      mkdir(path.c_str(), 0755);
    }
  }

  // 闲置检测线程
  void idleCheckLoop() {
    while (running) {
      std::this_thread::sleep_for(std::chrono::seconds(1));

      auto now = std::chrono::system_clock::now();
      auto elapsed =
          std::chrono::duration_cast<std::chrono::seconds>(now - lastKeyTime)
              .count();

      if (elapsed >= 10) {
        saveKeyCountsToFile();
      }
    }
  }

  // 日期检测线程
  void dateCheckLoop() {
    int lastDay = -1;
    
    while (running) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      
      std::tm utc8_tm = getUTCP8Time();
      int currentDay = utc8_tm.tm_mday;
      
      if (lastDay != -1 && currentDay != lastDay) {
        // 日期变化，保存当前数据并更新文件名
        saveKeyCountsToFile();
        updateFilename();
      }
      
      lastDay = currentDay;
    }
  }

  // 更新文件名
  void updateFilename() {
    std::lock_guard<std::mutex> lock(filenameMutex);
    currentFilename = generateFilename();
  }

  // 打印当前计数
  void printCurrentCounts() {
    std::lock_guard<std::mutex> lock(dataMutex);

    std::cout << "\n=== 当前按键统计 ===" << std::endl;
    std::cout << "修饰键:" << std::endl;
    for (const auto &m : modifierCounts) {
      if (modifierKeys.count(m.first)) {
        std::cout << modifierKeys.at(m.first) << ": " << m.second << std::endl;
      }
    }

    std::cout << "\n普通键:" << std::endl;
    for (const auto &k : keyCounts) {
      if (keyNames.count(k.first)) {
        std::cout << keyNames.at(k.first) << "(" << k.first << "): " << k.second
                  << std::endl;
      } else {
        std::cout << "Key(" << k.first << "): " << k.second << std::endl;
      }
    }
    std::cout << "==================\n" << std::endl;
  }

  void loadKeyCountsFromFile() {
    std::lock_guard<std::mutex> lock(filenameMutex);
    // check if file exist
    // if not, pass
    struct stat buffer;
    if (stat(currentFilename.c_str(), &buffer) == 0) {
      FILE *file = fopen(currentFilename.c_str(), "r");
      if (file) {
        int keycode, count;
        while (fscanf(file, "%d: %d\n", &keycode, &count) == 2) {
          keyCounts[keycode] = count;
        }
        fclose(file);
      } else {
        std::cerr << "无法打开文件进行读取: " << currentFilename << std::endl;
      }
    } else {
      std::cerr << "文件不存在: " << currentFilename << std::endl;
    }
  }

public:
  KeyMonitor(std::string dir) : logDir(dir) {
    ensureDirectoryExists(logDir);
    currentFilename = generateFilename();
    loadKeyCountsFromFile();
    lastKeyTime = std::chrono::system_clock::now();
    idleCheckThread = std::thread(&KeyMonitor::idleCheckLoop, this);
    dateCheckThread = std::thread(&KeyMonitor::dateCheckLoop, this);
  }

  ~KeyMonitor() {
    running = false;
    if (idleCheckThread.joinable()) {
      idleCheckThread.join();
    }
    if (dateCheckThread.joinable()) {
      dateCheckThread.join();
    }
    printCurrentCounts();
  }

  void setEventTap(CFMachPortRef tap) { eventTap = tap; }
  CFMachPortRef getEventTap() const { return eventTap; }

  void handleEvent(CGEventType type, CGEventRef event) {
    std::lock_guard<std::mutex> lock(dataMutex);
    lastKeyTime = std::chrono::system_clock::now();

    const int64_t keycode =
        CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);

    if (type == kCGEventFlagsChanged) {
      const CGEventFlags newFlags = CGEventGetFlags(event);

      // 统计修饰键按下次数
      if (modifierKeys.count(keycode)) {
        modifierCounts[keycode]++;
        std::cout << "修饰键按下: " << modifierKeys.at(keycode) << std::endl;
      }

      currentFlags = newFlags;
      return;
    }

    if (type == kCGEventKeyDown) {
      keyCounts[keycode]++;

      UniChar chars[4];
      UniCharCount len = 0;
      CGEventKeyboardGetUnicodeString(event, 4, &len, chars);

      if (keyNames.count(keycode)) {
        std::cout << "按键: " << keyNames.at(keycode) << std::endl;
      } else if (len > 0 && std::isprint(chars[0])) {
        std::cout << "按键: "
                  << std::string(reinterpret_cast<char *>(chars), len)
                  << std::endl;
      } else {
        std::cout << "按键: Key(" << keycode << ")" << std::endl;
      }

      isKeyDownProcessed = true;
    }

    if (type == kCGEventKeyUp && isKeyDownProcessed) {
      isKeyDownProcessed = false;
      currentFlags = CGEventGetFlags(event);
    }
  }

  void saveKeyCountsToFile() {
    std::lock_guard<std::mutex> lock(dataMutex);
    std::lock_guard<std::mutex> lockFile(filenameMutex);

    FILE *file = fopen(currentFilename.c_str(), "w");
    if (file) {
      for (const auto &pair : keyCounts) {
        fprintf(file, "%d: %d\n", pair.first, pair.second);
      }
      fclose(file);
    } else {
      std::cerr << "无法打开文件进行写入: " << currentFilename << std::endl;
    }
  }
};

// 全局变量用于信号处理
std::atomic<bool> g_shouldExit{false};

// 信号处理函数
void handleSignal(int signal) {
  if (signal == SIGINT) {
    g_shouldExit = true;
  }
}

// 事件回调
CGEventRef eventCallback(CGEventTapProxy proxy, CGEventType type,
                         CGEventRef event, void *refcon) {
  KeyMonitor *monitor = static_cast<KeyMonitor *>(refcon);

  if (type == kCGEventTapDisabledByTimeout) {
    CGEventTapEnable(monitor->getEventTap(), true);
    return event;
  }

  monitor->handleEvent(type, event);
  return event;
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "用法: " << argv[0] << " <日志目录>" << std::endl;
    return 1;
  }

  // 设置信号处理
  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = handleSignal;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, nullptr);

  KeyMonitor monitor(argv[1]);

  CFMachPortRef eventTap = CGEventTapCreate(
      kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault,
      CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp) |
          CGEventMaskBit(kCGEventFlagsChanged),
      eventCallback, &monitor);

  monitor.setEventTap(eventTap);

  if (!eventTap) {
    std::cerr << "请开启辅助功能权限！" << std::endl;
    return 1;
  }

  CFRunLoopSourceRef runLoopSource =
      CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
  CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource,
                     kCFRunLoopCommonModes);
  CGEventTapEnable(eventTap, true);

  std::cout << "开始监听（Control-C退出）..." << std::endl;
  std::cout << "日志文件: " << argv[1] << "/yyyy-mm-dd.log" << std::endl;

  // 修改后的运行循环，检查退出标志
  while (!g_shouldExit) {
    CFRunLoopRunResult result =
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.1, true);

    // 处理超时情况
    if (result == kCFRunLoopRunTimedOut) {
      continue; // 继续循环
    }

    // 处理其他情况
    if (result == kCFRunLoopRunStopped || result == kCFRunLoopRunFinished) {
      break;
    }
  }

  if (g_shouldExit) {
    std::cout << "\n检测到退出信号，正在保存数据..." << std::endl;
    monitor.saveKeyCountsToFile();
  }
  std::cout << "\n程序退出，打印最终统计结果..." << std::endl;

  CFRelease(runLoopSource);
  CFRelease(eventTap);
  return 0;
}
