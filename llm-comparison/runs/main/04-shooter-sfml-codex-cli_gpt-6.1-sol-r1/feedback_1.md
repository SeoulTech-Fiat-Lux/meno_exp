빌드에 실패했습니다. 컴파일러 출력의 앞부분입니다.

```text
main.cpp:104:53: error: ‘KeyCount’ is not a member of ‘sf::Keyboard::Key’
  104 |         static_cast<std::size_t>(sf::Keyboard::Key::KeyCount);
      |                                                     ^~~~~~~~
main.cpp:106:10: error: ‘<expression error>’ in namespace ‘std’ does not name a type
  106 |     std::array<bool, Count> held{};
      |          ^~~~~~~~~~~~~~~~~~
main.cpp:107:10: error: ‘<expression error>’ in namespace ‘std’ does not name a type
  107 |     std::array<bool, Count> tapped{};
      |          ^~~~~~~~~~~~~~~~~~
main.cpp: In member function ‘void {anonymous}::Input::key(sf::Keyboard::Key, bool)’:
main.cpp:117:9: error: ‘held’ was not declared in this scope
  117 |         held[static_cast<std::size_t>(index)] = down;
      |         ^~~~
main.cpp:119:13: error: ‘tapped’ was not declared in this scope
  119 |             tapped[static_cast<std::size_t>(index)] = true;
      |             ^~~~~~
main.cpp: In member function ‘bool {anonymous}::Input::active(sf::Keyboard::Key) const’:
main.cpp:125:16: error: ‘held’ was not declared in this scope
  125 |         return held[index] || tapped[index];
      |                ^~~~
main.cpp:125:31: error: ‘tapped’ was not declared in this scope
  125 |         return held[index] || tapped[index];
      |                               ^~~~~~
main.cpp: In member function ‘void {anonymous}::Input::consume()’:
main.cpp:130:9: error: ‘tapped’ was not declared in this scope
  130 |         tapped.fill(false);
      |         ^~~~~~
main.cpp: In member function ‘void {anonymous}::Input::clear()’:
main.cpp:136:9: error: ‘held’ was not declared in this scope
  136 |         held.fill(false);
      |         ^~~~
```

오류를 고친 main.cpp 전체를 ```cpp 코드 블록 하나로 다시 보내 주세요.
