#include <Functions/FunctionHelpers.h>
#include <Functions/FunctionFactory.h>

#include <BridgeHelper/CatBoostLibraryBridgeHelper.h>
#include <BridgeHelper/IBridgeHelper.h>
#include <Columns/ColumnNullable.h>
#include <Columns/ColumnString.h>
#include <Columns/ColumnTuple.h>
#include <Columns/ColumnsNumber.h>
#include <Common/assert_cast.h>
#include <DataTypes/DataTypeNullable.h>
#include <DataTypes/DataTypeTuple.h>
#include <DataTypes/DataTypesNumber.h>
#include <Functions/IFunction.h>
#include <Interpreters/Context.h>
#include <Interpreters/Context_fwd.h>

#include "Functions/ggmlEvaluate/IGgmlModel.h"
#include "gpt_common.h"
#include "gptj.h"

#include <Common/Exception.h>
#include <Common/re2.h>
#include <DataTypes/DataTypeString.h>
#include "DataTypes/IDataType.h"

namespace DB
{

namespace ErrorCodes
{
    extern const int FILE_DOESNT_EXIST;
    extern const int ILLEGAL_TYPE_OF_ARGUMENT;
    extern const int TOO_FEW_ARGUMENTS_FOR_FUNCTION;
    extern const int TOO_MANY_ARGUMENTS_FOR_FUNCTION;
    extern const int ILLEGAL_COLUMN;
    extern const int SYNTAX_ERROR;
    extern const int LOGICAL_ERROR;
}

/// Evaluate GGML model.
/// - Arguments: TBD
/// - Result: TBD
class FunctionGGMLEvaluate final : public IFunction, WithContext
{
public:
    static constexpr auto name = "ggmlEvaluate";

    static FunctionPtr create(ContextPtr context_) { return std::make_shared<FunctionGGMLEvaluate>(context_); }

    explicit FunctionGGMLEvaluate(ContextPtr context_) : WithContext(context_) {}
    String getName() const override { return name; }
    bool isVariadic() const override { return true; }
    bool isSuitableForShortCircuitArgumentsExecution(const DataTypesWithConstInfo & /*arguments*/) const override { return true; }
    bool isDeterministic() const override { return false; }
    bool useDefaultImplementationForNulls() const override { return false; }
    size_t getNumberOfArguments() const override { return 0; }

    DataTypePtr getReturnTypeImpl(const ColumnsWithTypeAndName & arguments) const override
    {
        if (arguments.size() < 3)
            throw Exception(ErrorCodes::TOO_FEW_ARGUMENTS_FOR_FUNCTION, "Function {} expects exactly 3 arguments", getName());
        if (arguments.size() > 3)
            throw Exception(ErrorCodes::TOO_MANY_ARGUMENTS_FOR_FUNCTION, "Function {} expects exactly 3 arguments", getName());
        std::cout << __FUNCTION__ << " " << arguments[0].type->getName() << ' ' << arguments[1].type->getName() << ' ' << arguments[2].type->getName() << std::endl;  // GGMLTODO : remove log
        // TODO : validate types
        // const auto * name_col = checkAndGetColumn<ColumnString>(arguments[0].column.get());
        // if (!name_col)
        //     throw Exception(ErrorCodes::ILLEGAL_COLUMN, "Argument of function {} must be a string", getName());
        return std::make_shared<DataTypeString>();
    }

    ColumnPtr executeImpl(const ColumnsWithTypeAndName & arguments, const DataTypePtr & result_type, size_t input_rows_count) const override
    {
        std::cout << "GGML!!!" << std::endl;  // GGMLTODO : remove log
        std::cout << "input_rows_count is : " << input_rows_count << std::endl;  // GGMLTODO : remove log
        std::cout << "result_type is : " << result_type->getName() << std::endl;  // GGMLTODO : remove log
        if (input_rows_count == 0) {
            ColumnPtr res = arguments[0].column;
            return res;
        }

        std::string model_path;
        {
            const auto& model_path_arg = *arguments[0].column.get();
            auto val = model_path_arg[0];
            if (!val.tryGet(model_path)) {
                throw Exception(ErrorCodes::SYNTAX_ERROR, "No2");
            }

            model_path = /* get prefix from config + */ "/home/ArtNext/" + model_path;
        }
        std::cout << "Deduced model path to be " << model_path << std::endl;  // GGMLTODO : remove log
        std::tuple<Int32> params;
        {
            auto val = (*arguments[1].column.get())[0];
            Tuple t;
            if (!val.tryGet(t)) {
                throw Exception(ErrorCodes::SYNTAX_ERROR, "No2");
            }
            UInt64 n_predict = t[0].safeGet<UInt64>();
            params = { n_predict };
            std::cout << "Deduced n_predict as " << n_predict << std::endl;  // GGMLTODO : remove log
        }
        std::cout << "Deduced params to be " << std::get<0>(params) << std::endl; // GGMLTODO : remove log

        auto model = getModel(model_path);

        std::cout << "loaded\n";  // GGMLTODO : remove log

        const auto& vals = *arguments[2].column.get();
        auto col_res = ColumnString::create();
        col_res->reserve(input_rows_count);
        UInt64 totalsize = 0;
        std::vector<String> result_raw(input_rows_count);

        for (size_t j = 0; j < input_rows_count; ++j) {
            Field field = vals[j]; // get(i, field);
            std::string val;
            if (!field.tryGet(val)) {
                throw Exception(ErrorCodes::SYNTAX_ERROR, "Nasrali");
            }
            else {
                std::cout << "Processing " << val << '\n';  // GGMLTODO : remove log
                std::string result = model->eval(val);
                result_raw[j] = std::move(result);
                totalsize += result_raw[j].size() + 1;
            }
        }

        col_res->getChars().resize(totalsize);
        col_res->getOffsets().resize(input_rows_count);
        auto* data_ptr = col_res->getChars().data();
        UInt64 offset = 0;
        for (size_t i = 0; i < input_rows_count; ++i) {
            memcpy(data_ptr + offset, result_raw[i].data(), result_raw[i].size());
            data_ptr[offset + result_raw[i].size()] = '\0';
            offset += result_raw[i].size() + 1;
            col_res->getOffsets()[i] = offset;
        }

        std::cout << "Success!!!" << std::endl; // GGMLTODO : remove log
        return col_res;
    }

private:
    std::shared_ptr<IGgmlModel> getModel(const std::string & path) const
    {
        auto & storage = getContext()->getGgmlModelStorage();
        auto model = storage.get("gptj", []() { return std::make_shared<GptJModel>(); });
        model->load(path);
        return model;
    }
};


REGISTER_FUNCTION(GGMLEvaluate)
{
    factory.registerFunction<FunctionGGMLEvaluate>();
}

}
