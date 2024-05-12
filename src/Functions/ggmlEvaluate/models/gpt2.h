#include <map>
#include <Functions/ggmlEvaluate/IGgmlModel.h>
#include "Functions/ggmlEvaluate/models/gpt_common.h"
#include "ggml-alloc.h"
#include "ggml-backend.h"

namespace DB
{

// default hparams (GPT-2 117M)
struct Gpt2Hparams
{
    int32_t n_vocab = 50257;
    int32_t n_ctx = 1024;
    int32_t n_embd = 768;
    int32_t n_head = 12;
    int32_t n_layer = 12;
    int32_t ftype = 1;
    float eps = 1e-5f;
};

struct Gpt2Layer
{
    // normalization
    struct ggml_tensor * ln_1_g;
    struct ggml_tensor * ln_1_b;

    struct ggml_tensor * ln_2_g;
    struct ggml_tensor * ln_2_b;

    // attention
    struct ggml_tensor * c_attn_attn_w;
    struct ggml_tensor * c_attn_attn_b;

    struct ggml_tensor * c_attn_proj_w;
    struct ggml_tensor * c_attn_proj_b;

    // mlp
    struct ggml_tensor * c_mlp_fc_w;
    struct ggml_tensor * c_mlp_fc_b;

    struct ggml_tensor * c_mlp_proj_w;
    struct ggml_tensor * c_mlp_proj_b;
};

struct Gpt2ModelState
{
    Gpt2Hparams hparams;

    // normalization
    struct ggml_tensor * ln_f_g;
    struct ggml_tensor * ln_f_b;

    struct ggml_tensor * wte; // position embedding
    struct ggml_tensor * wpe; //    token embedding
    struct ggml_tensor * lm_head; // language model head

    std::vector<Gpt2Layer> layers;

    // key + value memory
    struct ggml_tensor * memory_k;
    struct ggml_tensor * memory_v;

    //
    struct ggml_context * ctx_w;
    struct ggml_context * ctx_kv;

    ggml_backend_t backend = nullptr;

    ggml_backend_buffer_t buffer_w;
    ggml_backend_buffer_t buffer_kv;

    std::map<std::string, struct ggml_tensor *> tensors;
};

class Gpt2Model : public IGgmlModel
{
public:
    ~Gpt2Model() override
    {
        ggml_gallocr_free(allocr);
        ggml_backend_buffer_free(state.buffer_w);
        ggml_backend_buffer_free(state.buffer_kv);
        ggml_backend_free(state.backend);

        ggml_free(state.ctx_w);
    }

private:
    void loadImpl(ConfigPtr config) override;
    std::string evalImpl(std::tuple<Int32> user_params, const std::string & input) override;
    ggml_cgraph * gpt2_graph(int n_past, int n_tokens);
    bool gpt2_eval(int n_threads, int n_past, std::vector<GptVocab::id> & embd_inp, std::vector<float> & embd_w);
    GptVocab gpt_vocab;
    Gpt2ModelState state;
    GptParams gpt_params;
    ggml_gallocr_t allocr = nullptr;
};

}
