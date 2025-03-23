#include <ncurses.h>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>
#include <format>
#include <memory>
#include <unordered_map>
#include <span>
#include <initializer_list>
#include <type_traits>

#define STACK_WIDTH 25
#define INPUT_WIDTH 40

namespace calc {
    enum class WinOpt {
        BOXED, NOBOXED,
        TITLED, NOTITLED,
        LALIGNED, CALIGNED, RALIGNED
    };

    class Curses {
    public:
        Curses() {
            initscr();
            cbreak();
            noecho();
            keypad(stdscr, TRUE);
            curs_set(0);
            raw();
            if (has_colors()) {
                start_color();
                init_pair(1, COLOR_WHITE, COLOR_BLUE);
                init_pair(2, COLOR_WHITE, COLOR_BLACK);
                init_pair(3, COLOR_BLACK, COLOR_WHITE);
                init_pair(4, COLOR_BLACK, COLOR_YELLOW);
                init_pair(5, COLOR_BLACK, COLOR_CYAN);
                init_pair(6, COLOR_BLACK, COLOR_MAGENTA);
                init_pair(7, COLOR_BLACK, COLOR_GREEN);
                init_pair(8, COLOR_BLACK, COLOR_RED);
            }
            refresh();
        }

        ~Curses() { endwin(); }
    };

    class Item {
    public:
        Item() = default;
        Item(std::string v) { value(v); }
        Item(std::function<std::string()> func) { value(func);}

        std::string value() const {
            if (_dynamic) { return _dynValue(); }
            return _value;
        }

        void value(std::string v) {
            _value = v;
            _dynamic = false;
            enable();
        }

        void value(std::function<std::string()> func) {
            _dynValue = func;
            _dynamic = true;
            enable();
        }

        void enable() { _enabled = true; }
        void disable() { _enabled = false; }
        bool enabled() { return _enabled; }

        void print(WINDOW* win, int row, int col, int width, int height = 1){
            if (!_enabled) { return; }

            int length = value().size();
            int x = (width - length) /2;
            
            mvwprintw(win, row, x, value().c_str());
        }

    private:
        std::string _value;
        std::function<std::string()> _dynValue;

        bool _dynamic = false;
        bool _enabled = false;
    };

    class Content {
    public:
        Content() = default;

        std::vector<Item>& operator[](int row) { return _content[row]; }
        const std::vector<Item>& operator[](int row) const { return _content[row]; }

        std::vector<int> size() const { return { _rows, _cols }; }

        void add(std::string value) {
            allocateNew();
        }

        void add(std::function<std::string()> func) {
            allocateNew();
        }

        void allocateNew() {
            if (_content.empty()) _content.push_back(std::vector<Item>());
            if (_content[_rptr].empty()) _content[_rptr].push_back(Item());
        }

        void replace(int row, int col, std::string value) { _content[row][col].value(value); }
        void replace(int row, int col, std::function<std::string()> func) { _content[row][col].value(func); }

        void print(WINDOW* win, std::vector<int> coords){
            int h = coords[0], w = coords[1];
            int y = coords[2], x = coords[3];
        }

    private:
        int _rows = -1;
        int _cols = 1;

        int _rptr = 0;
        int _cptr = 0;

        std::vector<std::vector<Item>> _content;
    };

    class Window {
    public:
        Window() {}

        Window(std::string name, std::function<std::vector<int>()> func) :
            _name(name), _getDims(func)
        {
            std::vector<int> d = getDims();

            _win = newwin(d[0], d[1], d[2], d[3]);
            wattron(_win, COLOR_PAIR(1));
            wbkgd(_win, COLOR_PAIR(1));
            draw();
        }

        void setOption(WinOpt opt) {
            switch (opt) {
                case WinOpt::BOXED: _boxed = true; break;
                case WinOpt::NOBOXED: _boxed = false; break;
                case WinOpt::TITLED: _titled = true; break;
                case WinOpt::NOTITLED: _titled = false; break;
                case WinOpt::LALIGNED: _title_align = "left"; break;
                case WinOpt::CALIGNED: _title_align = "center"; break;
                case WinOpt::RALIGNED: _title_align = "right"; break;
            }
            this->resize();
        }

        std::vector<int> getCellWidth(int n){
            int w = getmaxx(_win);
            if (_boxed) { w -= 2; }
            return {w / n, w % n};
        }

        void markSelected() {
            _selected = true;

            draw();

            wnoutrefresh(_win);
            doupdate();
        }

        void resize() {
            std::vector<int> d = getDims();

            wclear(_win);
            mvwin(_win, d[2], d[3]);
            wresize(_win, d[0], d[1]);

            draw();
        }

        void refresh() { wnoutrefresh(_win); }

        void unmarkSelected() {
            _selected = false;
            draw();
        }

        WINDOW* win() { return _win; }

        std::vector<int> contentSize() { return _content.size(); }
        
        void contentAdd(std::string value) { _content.add(value); resize(); }
        
        void contentAdd(std::function<std::string()> func) { _content.add(func); resize(); }

    private:
        std::function<std::vector<int>()> _getDims;
        std::vector<int> getDims() { return _getDims(); }

        void draw(){
            drawBox();

            putTitle();

            _content.print(_win, getCoords());

            this->refresh();
        }

        std::vector<int> getCoords() {
            std::vector<int> coords {getmaxy(_win), getmaxx(_win), 0, 0};

            if (_boxed) { coords[0] -= 2; coords[1] -= 2; coords[2] += 1; coords[3] += 1; }

            return coords;
        }

        void drawBox() {
            if (!_boxed) { return; }
            box(_win, 0, 0);
        }

        void putTitle() {
            int startx = 0;
            std::string title = " " + _name + " ";

            if (!_titled) { return; }

            if (_title_align == "left"){ startx = 2;}
            if (_title_align == "center"){ startx = (getmaxx(_win) - title.size()) / 2; }
            if (_title_align == "right"){ startx = getmaxx(_win) - title.size() - 2; }

            if (_selected) { wattron(_win, A_REVERSE); }
            mvwprintw(_win, 0, startx, title.c_str());
            if (_selected) { wattroff(_win, A_REVERSE); }
        }

        Content _content;
        WINDOW* _win;
        std::string _name;
        std::string _title_align = "left";
        bool _boxed = true;
        bool _selected = false;
        bool _titled = true;
    };

    class Windows {
    public:
        Windows() = default;

        Window& operator[](std::string name) { return _windows[name]; }
        const Window& operator[](std::string name) const { return _windows.at(name); }

        void notify(std::string message) {
            if (message == "resize") { resizeAllWindows(); }
            if (message == "rotate") { rotateSelectedWindow("right"); }
            if (message == "rotate-back") { rotateSelectedWindow("left"); }
        }

        void windowsOrder(std::vector<std::string> order) { _windowsOrder = order; }
        void windowsOrder(std::string name) { _windowsOrder.push_back(name); }

    private:
        std::string getSelectedWindow() { return _windowsOrder[0]; }


        void updateMarkedWindow() {
            if (_windowsOrder.size() == 0) { return; }

            _windows[_windowsOrder[0]].markSelected();

            for (int i = 1; i < _windowsOrder.size(); i++) { _windows[_windowsOrder[i]].unmarkSelected(); }
        }

        void resizeAllWindows() {
            wnoutrefresh(stdscr);
            for (auto& w : _windows) { w.second.resize(); }
        }

        void rotateSelectedWindow(std::string direction) {
            if (direction == "right") { rotateRightSelectedWindow(); }
            if (direction == "left") { rotateLeftSelectedWindow(); }
            updateMarkedWindow();
        }

        void rotateRightSelectedWindow() { std::rotate(_windowsOrder.rbegin(), _windowsOrder.rbegin() + 1, _windowsOrder.rend()); }

        void rotateLeftSelectedWindow() { std::rotate(_windowsOrder.begin(), _windowsOrder.begin() + 1, _windowsOrder.end()); }

        std::unordered_map<std::string, Window> _windows;
        std::vector<std::string> _windowsOrder;
    };

    class App {
    public:
        App() : curses(), _windows() { }

        void run() {
            windows().notify("rotate");
            int c;
            while ((c = getch()) != 'q') {
                if (c == KEY_RESIZE) { windows().notify("resize"); }
                if (c == '\t') { windows().notify("rotate"); }
                if (c == KEY_BTAB) { windows().notify("rotate-back"); }
                doupdate();
            }
        }

        template<typename... WinOpts, typename = std::enable_if_t<(std::is_same_v<WinOpts, WinOpt> && ...)>>
        void addWindow(std::string name, std::function<std::vector<int>()> func, WinOpts... opts) {
            windows()[name] = Window(name, func);
            
            (..., windows()[name].setOption(opts));

            windows().windowsOrder(name);
            doupdate();
        }

        void setWindowsOrder(std::vector<std::string> order) { windows().windowsOrder(order); }

        Window& window(std::string name) { return windows()[name]; }

        Windows& windows() { return _windows; }

    private:
        Curses curses;
        Windows _windows;
    };
}

void initWindows(calc::App& app){
    wnoutrefresh(stdscr);
    app.addWindow("Menu",
        []() -> std::vector<int> { return {1, getmaxx(stdscr), 0, 0}; }, calc::WinOpt::NOBOXED, calc::WinOpt::NOTITLED);
    app.addWindow("Status",
        []() -> std::vector<int> { return {1, getmaxx(stdscr), getmaxy(stdscr) - 1, 0}; }, calc::WinOpt::NOBOXED, calc::WinOpt::NOTITLED);
    app.addWindow("Stack",
        []() -> std::vector<int> { return {getmaxy(stdscr) - 2, STACK_WIDTH, 1, 0}; });
    app.addWindow("Input",
        []() -> std::vector<int> { return {3, INPUT_WIDTH, 1, STACK_WIDTH}; });
    app.addWindow("Result",
        []() -> std::vector<int> { return {3, getmaxx(stdscr) - STACK_WIDTH - INPUT_WIDTH, 1, STACK_WIDTH + INPUT_WIDTH}; });
    app.addWindow("Ops",
        []() -> std::vector<int> { return {getmaxy(stdscr) - 5, INPUT_WIDTH, 4, STACK_WIDTH}; });
    app.addWindow("Vars",
        []() -> std::vector<int> { return {getmaxy(stdscr) - 5, getmaxx(stdscr) - STACK_WIDTH - INPUT_WIDTH, 4, STACK_WIDTH + INPUT_WIDTH}; });
    app.setWindowsOrder({"Stack", "Ops", "Vars", "Result", "Input"});
}

void initContent(calc::App& app){
    calc::Window& m = app.window("Vars");
    calc::Window& s = app.window("Status");
    std::function<std::string()> f1 = [&]() -> std::string {
        return std::format(
            "([{} , {}], [{}, {}]) {} {}",
            getbegy(m.win()), getbegx(m.win()), getmaxy(m.win()), getmaxx(m.win()), m.contentSize()[0], m.contentSize()[1]
        );
    };
    m.contentAdd("holi");
    s.contentAdd(f1);
}


int main() {
    calc::App app;

    initWindows(app);
    initContent(app);

    app.run();

    return 0;
}
