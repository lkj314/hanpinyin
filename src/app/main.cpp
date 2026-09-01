// HanPinyin · 设置 / 注册 exe 入口（WinMain）
// 启动 ConfigApp：系统托盘 + 自注册/卸载 + 配置面板。绿色解压后右键管理员运行一次即可。

#include "app_context.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                   LPSTR lpCmdLine, int nCmdShow) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    hanpinyin::ConfigApp app;
    return app.run();
}
