빌드에 실패했습니다. 컴파일러 출력의 앞부분입니다.

```text
main.cpp: In member function ‘void {anonymous}::Game::draw(sf::RenderWindow&)’:
main.cpp:139:34: error: ‘kWinW’ was not declared in this scope
  139 |             placeCentered(title, kWinW / 2.f, 220.f);
      |                                  ^~~~~
main.cpp:152:37: error: ‘kWinW’ was not declared in this scope
  152 |             sf::RectangleShape bar({kWinW, kHud});
      |                                     ^~~~~
main.cpp:152:49: error: no matching function for call to ‘sf::RectangleShape::RectangleShape(<brace-enclosed initializer list>)’
  152 |             sf::RectangleShape bar({kWinW, kHud});
      |                                                 ^
In file included from build/_deps/sfml-src/include/SFML/Graphics.hpp:41,
                 from main.cpp:2:
build/_deps/sfml-src/include/SFML/Graphics/RectangleShape.hpp:54:14: note: candidate: ‘sf::RectangleShape::RectangleShape(sf::Vector2f)’
   54 |     explicit RectangleShape(Vector2f size = {});
      |              ^~~~~~~~~~~~~~
build/_deps/sfml-src/include/SFML/Graphics/RectangleShape.hpp:54:38: note:   no known conversion for argument 1 from ‘<brace-enclosed initializer list>’ to ‘sf::Vector2f’ {aka ‘sf::Vector2<float>’}
   54 |     explicit RectangleShape(Vector2f size = {});
      |                             ~~~~~~~~~^~~~~~~~~
build/_deps/sfml-src/include/SFML/Graphics/RectangleShape.hpp:45:25: note: candidate: ‘constexpr sf::RectangleShape::RectangleShape(const sf::RectangleShape&)’
   45 | class SFML_GRAPHICS_API RectangleShape : public Shape
      |                         ^~~~~~~~~~~~~~
build/_deps/sfml-src/include/SFML/Graphics/RectangleShape.hpp:45:25: note:   no known conversion for argument 1 from ‘<brace-enclosed initializer list>’ to ‘const sf::RectangleShape&’
build/_deps/sfml-src/include/SFML/Graphics/RectangleShape.hpp:45:25: note: candidate: ‘constexpr sf::RectangleShape::RectangleShape(sf::RectangleShape&&)’
build/_deps/sfml-src/include/SFML/Graphics/RectangleShape.hpp:45:25: note:   no known conversion for argument 1 from ‘<brace-enclosed initializer list>’ to ‘sf::RectangleShape&&’
main.cpp:185:33: error: ‘kWinW’ was not declared in this scope
  185 |             placeCentered(over, kWinW / 2.f, 200.f);
      |                                 ^~~~~
```

오류를 고친 main.cpp 전체를 ```cpp 코드 블록 하나로 다시 보내 주세요.
