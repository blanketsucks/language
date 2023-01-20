#include "visitor.h"

void Visitor::panic(const std::string& message, Span span) {
    llvm::Function* function = this->module->getFunction("__quart_panic"); // Defined in lib/panic.c
    if (!function) {
        function = this->create_function(
            "__quart_panic",
            this->builder->getVoidTy(),
            {
                this->builder->getInt8PtrTy(), // file
                this->builder->getInt32Ty(),   // line
                this->builder->getInt32Ty(),   // column
                this->builder->getInt8PtrTy()  // message
            },
            false,
            llvm::Function::LinkageTypes::ExternalLinkage
        );

        function->setDoesNotReturn();
    }

    this->builder->CreateCall(function, {
        this->to_str(span.filename), 
        this->to_int(span.start.line),
        this->to_int(span.start.column),
        this->to_str(message)
    });

    this->builder->CreateUnreachable();
}