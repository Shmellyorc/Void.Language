extern "C" int c_add(int, int);
class Object { public: int method(int); };
int overloaded(int);
double overloaded(double);
template<class T> T generic(T);
namespace hidden { extern "C" int nested(int); }
extern "C" bool c_positive(int) noexcept;
struct CPoint { int x; double y; };
extern "C" CPoint c_shift(CPoint);
