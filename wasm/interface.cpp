#include <pdfla/pdfla.h>
#include <emscripten.h>
#include <emscripten/bind.h>

using namespace emscripten;
using namespace Targoman::PDFLA;

EMSCRIPTEN_BINDINGS(PDFLAJS)
{
  register_vector<Targoman::DLA::clsDocBlockPtr>("DocBlockPtrVector_t");
  class_<clsPdfLa>("clsPdfLa")
    .property("PageCount", &clsPdfLa::pageCount)
    .function("getPageBlocks", &clsPdfLa::getPageBlocks);
}

