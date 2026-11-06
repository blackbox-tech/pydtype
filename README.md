# pydtype

Compile time C++26 exposing C structs to numpy dtypes.  Supports inheritance, nested types, multi-dimensional arrays, 
enums, named and anonymous unions, and chrono to datetime64 and timedelta64 conversion. Where the numpy default 
alignment differs from C it will update the dtype to match.

This is currrently an early version that wrote during my day job as a quant which I've released because it maybe useful 
to the wider numpy community. I've only had time to test it with gcc16.2 on linux. To compile it I had to include the 
`-std=c++26 -freflection` flags.

I welcome contributions, especially to get it work on other compilers and platforms.

Thanks,

Paul

## Examples

```c++
#include <pydtype/desc.hpp>


struct Simple {
    std::uint32_t len;
    float coords[3];
    double value;
};

```

`pydtype::desc<Simple>()` will produce:
```
[('len', 'u4'), ('coords', ('f4', 3)), ('value', 'f8')]
```

Here is a more complex example:

```c++

#include <pydtype/desc.hpp>

enum class Type
    :   std::uint8_t {
    LINE = 0,
    TRANSFORM,
};

#pragma pack(push, 1)

struct Point {
    double x;
    double y;
    double z;
};


class Base {
public:
    std::chrono::time_point<std::int64_t, std::chrono::nanoseconds> time;
    char label[12];
};


class Complex 
    : public Base {
public:
    Type type;

    union {
        Point line[2];
        float transform[4][4];
    } data;
};

#pragma pack(pop) 
```

`pydtype::desc<Complex. pydtype::Endian::LITTLE>()` will produce:

```
{'names': ['time', 'label', 'type', 'data'], 'formats': ['<M8[ns]', 'S12', 'u1', {
    'names': ['line', 'transform'], 'formats': [([('x', '<f8'), ('y', '<f8'),('z', '<f8')], 2), ('<f4', 4, 4)], 
        'offsets': [0, 0],'itemsize': 64}], 
 'offsets': [0, 8, 20, 21], 'itemsize': 85}

```

Here is another example combining pydtype with the [pybind11](https://github.com/pybind/pybind11) library, returning raw 
C struct data from C++ to python so it can be accessed directly by numpy.

```c++
#include <pybind11/pybind11.h>
#include <pybind11/eval.h>
#include <pybind11/numpy.h>
#include <pydtype/desc.hpp>

namespace py = pybind11;

auto ptr = new Simple[10];
auto cap = py::capsule(ptr, [](void *v) { delete[] reinterpret_cast<Simple*>(v); });
auto dtype = py::dtype::from_args(py::eval(pydtype::desc<Simple>()));
return py::array(dtype, {10}, ptr, cap);
```

## Installation

You can use pip, uv and conda to install the pydtype header file into your current python environment.

### Installing from a local repos (development)

### pip
```commandline
git clone https://github.com/blackbox-tech/pydtype.git
cd pydtype
python -m pip install .
```

### uv
```commandline
git clone https://github.com/blackbox-tech/pydtype.git
cd pydtype
uv pip install .
```

### conda
```commandline
git clone https://github.com/blackbox-tech/pydtype.git
cd pydtype/conda
conda build .
conda install --use-local pydtype
```
