#pragma once

#include <functional>
#include <unordered_set>
#include <unordered_map>

#include "libspirv/libspirv.h"

#include "shaderanalyzer.h"

class MeshConverter {
  public:
    explicit MeshConverter(libspirv::MutableBytecode& code);
    libspirv::MutableBytecode& vertexPassthrough() { return vert; }

    void     exec();

  private:
    void     avoidReservedFixup();
    void     removeMultiview(libspirv::MutableBytecode& code);
    void     removeCullClip(libspirv::MutableBytecode& code);
    void     removeFromPerVertex(libspirv::MutableBytecode& code, const std::unordered_set<uint32_t>& fields);

    void     injectLoop(libspirv::MutableBytecode::Iterator& fn,
                        uint32_t varI, uint32_t begin, uint32_t end, uint32_t inc, std::function<void(libspirv::MutableBytecode::Iterator& fn)> body);
    void     injectEngineBindings(const uint32_t idMainFunc);
    void     injectCountingPass(const uint32_t idMainFunc);
    void     replaceEntryPoint(const uint32_t idMainFunc, const uint32_t idEngineMain);

    void     vsTypeRemaps(libspirv::MutableBytecode::Iterator& fn, std::unordered_map<uint32_t, uint32_t>& typeRemaps,
                          const libspirv::Bytecode::AccessChain* ids, uint32_t len);
    uint32_t mappingTable(libspirv::MutableBytecode::Iterator& fn, uint32_t arrType, uint32_t eltType);
    uint32_t declareGlPerVertex(libspirv::MutableBytecode::Iterator& fn);

    void     generateVsDefault();
    void     generateVsSplit();

    void     annotateVertexBuiltins(libspirv::MutableBytecode& vert, uint32_t idVertexIndex, uint32_t glPerVertexT);

    libspirv::MutableBytecode& code;
    ShaderAnalyzer             an;
    libspirv::MutableBytecode  vert;

    struct gl_MeshPerVertexNV {
      bool gl_Position  = false;
      bool gl_PointSize = false;
      } gl_MeshPerVertexNV;

    // meslet builtins
    uint32_t idGlPerVertex        = 0;
    uint32_t std450Import         = 0;
  };

