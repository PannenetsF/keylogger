#include <Carbon/Carbon.h>
#include <cctype>
#include <iostream>
#include <map>

class KeyMonitor {
private:
  CFMachPortRef eventTap = nullptr;
  CGEventFlags currentFlags = 0;
  bool isKeyDownProcessed = false;

  const std::map<int, std::string> modifierKeys = {
      {54, "RCmd"},  {55, "Cmd"},    {56, "Shift"}, {57, "Caps"}, {58, "LAlt"},
      {59, "LCtrl"}, {60, "RShift"}, {61, "RAlt"},  {62, "RCtrl"}};

  const std::map<int, std::string> keyNames = {
      {36, "Enter"}, {48, "Tab"},    {49, "Space"}, {51, "Delete"},
      {53, "Esc"},   {71, "Clear"},  {76, "Enter"}, {96, "F5"},
      {97, "F6"},    {98, "F7"},     {99, "F3"},    {100, "F8"},
      {101, "F9"},   {103, "F11"},   {105, "F13"},  {107, "F14"},
      {109, "F10"},  {113, "F12"},   {115, "Home"}, {116, "PgUp"},
      {117, "Del"},  {119, "End"},   {121, "PgDn"}, {122, "F4"},
      {123, "Left"}, {124, "Right"}, {125, "Down"}, {126, "Up"},
      {114, "Help"}, {118, "F2"},    {120, "F1"}};

  std::string getModifiers() const {
    std::string mods;
    if (currentFlags & kCGEventFlagMaskCommand)
      mods += "Cmd+";
    if (currentFlags & kCGEventFlagMaskShift)
      mods += "Shift+";
    if (currentFlags & kCGEventFlagMaskAlternate)
      mods += "Alt+"; // 统一显示为Alt
    if (currentFlags & kCGEventFlagMaskControl)
      mods += "Ctrl+";
    if (currentFlags & kCGEventFlagMaskAlphaShift) // Caps Lock状态
      mods += "Caps+";
    if (!mods.empty())
      mods.pop_back();
    return mods;
  }


public:
  void setEventTap(CFMachPortRef tap) { eventTap = tap; }
  CFMachPortRef getEventTap() const { return eventTap; }

  void handleEvent(CGEventType type, CGEventRef event) {
    const int64_t keycode =
        CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode);

    if (type == kCGEventFlagsChanged) {
      const CGEventFlags newFlags = CGEventGetFlags(event);

      // 特殊处理Caps Lock键
      if ((newFlags ^ currentFlags) & kCGEventFlagMaskAlphaShift) {
        std::cout << "Caps Lock状态: "
                  << ((newFlags & kCGEventFlagMaskAlphaShift) ? "开启" : "关闭")
                  << std::endl;
      }

      if (newFlags != currentFlags) {
        currentFlags = newFlags;
        if (modifierKeys.count(keycode)) {
          std::string modName = modifierKeys.at(keycode);
          if (modName == "Caps")
            return; // Caps Lock已有特殊处理
          std::cout << "修饰键更新: " << getModifiers() << std::endl;
        }
      }
      return;
    }
    if (type == kCGEventKeyDown) {
      UniChar chars[4];
      UniCharCount len = 0;
      CGEventKeyboardGetUnicodeString(event, 4, &len, chars);

      std::string output = getModifiers();
      if (!output.empty())
        output += "+";

      if (modifierKeys.count(keycode)) {
        output += modifierKeys.at(keycode);
      } else if (keyNames.count(keycode)) {
        output += keyNames.at(keycode);
      } else if (len > 0 && std::isprint(chars[0])) {
        output += std::string(reinterpret_cast<char *>(chars), len);
      } else {
        output += "Key(" + std::to_string(keycode) + ")";
      }

      std::cout << "ComboKey: " << output << std::endl;
      isKeyDownProcessed = true;
    }

    if (type == kCGEventKeyUp && isKeyDownProcessed) {
      isKeyDownProcessed = false;
      currentFlags = CGEventGetFlags(event);
    }
  }
};

// 完整的事件回调实现
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

int main() {
  KeyMonitor monitor;

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

  std::cout << "开始监听（Cmd+Q退出）..." << std::endl;
  CFRunLoopRun();

  CFRelease(runLoopSource);
  CFRelease(eventTap);
  return 0;
}

int _main() {
  KeyMonitor monitor;

  CFMachPortRef eventTap = CGEventTapCreate(
      kCGSessionEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault,
      CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp) |
          CGEventMaskBit(kCGEventFlagsChanged),
      eventCallback, &monitor);

  monitor.setEventTap(eventTap); // 设置eventTap引用

  if (!eventTap) {
    std::cerr << "请开启辅助功能权限！" << std::endl;
    return 1;
  }

  CFRunLoopSourceRef runLoopSource =
      CFMachPortCreateRunLoopSource(kCFAllocatorDefault, eventTap, 0);
  CFRunLoopAddSource(CFRunLoopGetCurrent(), runLoopSource,
                     kCFRunLoopCommonModes);
  CGEventTapEnable(eventTap, true);

  std::cout << "开始监听（Cmd+Q退出）..." << std::endl;
  CFRunLoopRun();

  CFRelease(runLoopSource);
  CFRelease(eventTap);
  return 0;
}
