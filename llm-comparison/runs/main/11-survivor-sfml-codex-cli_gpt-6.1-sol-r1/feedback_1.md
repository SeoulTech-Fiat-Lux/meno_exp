빌드에 실패했습니다. 컴파일러 출력의 앞부분입니다.

```text
main.cpp:798:53: error: ‘KeyCount’ is not a member of ‘sf::Keyboard::Key’
  798 |         static_cast<std::size_t>(sf::Keyboard::Key::KeyCount);
      |                                                     ^~~~~~~~
main.cpp:799:10: error: ‘<expression error>’ in namespace ‘std’ does not name a type
  799 |     std::array<bool, Count> held{};
      |          ^~~~~~~~~~~~~~~~~~
main.cpp:800:10: error: ‘<expression error>’ in namespace ‘std’ does not name a type
  800 |     std::array<bool, Count> taps{};
      |          ^~~~~~~~~~~~~~~~~~
main.cpp: In member function ‘void {anonymous}::MovementInput::press(sf::Keyboard::Key)’:
main.cpp:807:13: error: ‘held’ was not declared in this scope
  807 |             held[static_cast<std::size_t>(index)] = true;
      |             ^~~~
main.cpp:808:13: error: ‘taps’ was not declared in this scope
  808 |             taps[static_cast<std::size_t>(index)] = true;
      |             ^~~~
main.cpp: In member function ‘void {anonymous}::MovementInput::release(sf::Keyboard::Key)’:
main.cpp:816:13: error: ‘held’ was not declared in this scope
  816 |             held[static_cast<std::size_t>(index)] = false;
      |             ^~~~
main.cpp: In member function ‘bool {anonymous}::MovementInput::down(sf::Keyboard::Key) const’:
main.cpp:822:16: error: ‘held’ was not declared in this scope
  822 |         return held[index] || taps[index];
      |                ^~~~
main.cpp:822:31: error: ‘taps’ was not declared in this scope
  822 |         return held[index] || taps[index];
      |                               ^~~~
main.cpp: In member function ‘void {anonymous}::MovementInput::clear()’:
main.cpp:838:9: error: ‘held’ was not declared in this scope
  838 |         held.fill(false);
      |         ^~~~
main.cpp:839:9: error: ‘taps’ was not declared in this scope
  839 |         taps.fill(false);
      |         ^~~~
main.cpp: In function ‘int main()’:
main.cpp:939:39: error: ‘struct {anonymous}::MovementInput’ has no member named ‘taps’
  939 |                                 input.taps.fill(false);
      |                                       ^~~~
main.cpp:946:35: error: ‘struct {anonymous}::MovementInput’ has no member named ‘taps’
  946 |                             input.taps.fill(false);
      |                                   ^~~~
main.cpp:963:35: error: ‘struct {anonymous}::MovementInput’ has no member named ‘taps’
  963 |                             input.taps.fill(false);
      |                                   ^~~~
main.cpp:991:27: error: ‘struct {anonymous}::MovementInput’ has no member named ‘taps’
  991 |                     input.taps.fill(false);
      |                           ^~~~
main.cpp:1005:23: error: ‘struct {anonymous}::MovementInput’ has no member named ‘taps’
 1005 |                 input.taps.fill(false);
      |                       ^~~~
```

오류를 고친 main.cpp 전체를 ```cpp 코드 블록 하나로 다시 보내 주세요.
