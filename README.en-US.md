

# asio3
Header only c++ network library, based on c++ 20 coroutine and asio

## Design Philosophy
* Relies on C++20 coroutines, and depends on some additional C++23 features. Uses a single-threaded coroutine mode, with simplicity and ease of use as goals.
* No other namespaces are defined. Instead, functionality is added directly to the `asio` namespace. Users can redirect the namespace in their own code via `namespace net=asio;`, then uniformly use `net::xxx` for cleaner code.
* Avoids inheritance, favoring the composition pattern. Various features are encapsulated into independent API functions, allowing users to choose which ones to call.
* Encapsulated API functions are designed to support multiple calling styles simultaneously, such as callback, future, and coroutine.
* Property classes no longer use getter/setter methods. Instead, they use C++20 designated initializers, similar to JavaScript syntax, resulting in very clean code.
* Uses `concept` instead of SFINAE, and strives to leverage new C++23 features.
* Provides lightweight framework support.
