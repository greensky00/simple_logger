
Simple Logger
---
Logging library for C++ programs.

Author
---
Jung-Sang Ahn <jungsang.ahn@gmail.com>

How to use
---
Copy all files under [src](src) directory to your project, and then include [logger.h](src/logger.h).

Example code
---
Please refer to [basic_example.cc](example/basic_example.cc)

Features
---
* Multi-thread safe.
* Lightweight: observed up to 6M logs/second (by multi threads) on i7 8-thread machine.
* Different log levels and formats for file and display.
* Circular log file reclaiming.
* Auto compression (`tar.gz`) of old log files.
* Stack backtrace on crash or abort.
  * Linux and Mac only.
  * Linux: `addr2line` is needed.

Screenshot
---
* On display
![alt text](https://user-images.githubusercontent.com/5001031/42741399-bdba0612-8866-11e8-902c-0f52b2e70d00.jpg "Screenshot")

* On log file
```
2018-07-15T19:33:10.550_560-07:00 [448e] [====] Start logger: ./example_log.log (32 MB per file, up to 16 files)    [logger.cc:435, start()]
2018-07-15T19:33:10.550_659-07:00 [448e] [FATL] fatal error [basic_example.cc:29, main()]
2018-07-15T19:33:10.550_699-07:00 [448e] [ERRO] error   [basic_example.cc:30, main()]
2018-07-15T19:33:10.550_734-07:00 [448e] [WARN] warning [basic_example.cc:31, main()]
2018-07-15T19:33:10.550_774-07:00 [448e] [INFO] info    [basic_example.cc:32, main()]
2018-07-15T19:33:10.550_814-07:00 [448e] [DEBG] debug   [basic_example.cc:33, main()]
2018-07-15T19:33:10.550_854-07:00 [448e] [TRAC] trace   [basic_example.cc:34, main()]
2018-07-15T19:33:10.550_900-07:00 [448e] [====] Stop logger: ./example_log.log  [logger.cc:451, stop()]
```

Misc.
---
* There are false alarms by thread sanitizer. To suppress it, add `-DSUPPRESS_TSAN_FALSE_ALARMS=1` flag to `CXXFLAGS`.
