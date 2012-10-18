//===----------------------------------------------------------------------===//
//
// Copyright (c) 2012 The University of Utah
// All rights reserved.
//
// This file is distributed under the University of Illinois Open Source
// License.  See the file COPYING for details.
//
//===----------------------------------------------------------------------===//

#if HAVE_CONFIG_H
#  include <config.h>
#endif

#include "SubstituteClassTemplateParameter.h"

#include "clang/AST/ASTContext.h"
#include "clang/AST/RecursiveASTVisitor.h"

#include "TransformationManager.h"
#include <cstdio>
#include <iostream>
#include <vector>
#include "llvm/Support/raw_os_ostream.h"

using namespace clang;
using namespace llvm;

static const char *DescriptionMsg =
"This pass tries to replace template arguments with types used in code";

static RegisterTransformation<SubstituteClassTemplateParameter>
         Trans("substitute-class-template-param", DescriptionMsg);

static std::string ArgumentToString(const TemplateArgument *arg, const ASTContext &Ctx)
{
    PrintingPolicy Policy = Ctx.getPrintingPolicy();
    std::string argStr;
    raw_string_ostream out(argStr);
    arg->print(Policy, out);
    out << " ";
    return out.str();
}

class SubstituteClassTemplateParameterASTVisitor : public
  RecursiveASTVisitor<SubstituteClassTemplateParameterASTVisitor> {

public:
    explicit SubstituteClassTemplateParameterASTVisitor(
            SubstituteClassTemplateParameter *Instance)
        : ConsumerInstance(Instance)
    {}

    // We want to traverse template arguments of specializations
    // but want to traverse everywhere else
//    bool TraverseTemplateArgumentLoc(const TemplateArgumentLoc& argloc) {
//        return true;
//    }

    bool TraverseTemplateDecl(const TemplateDecl *D) {
        return true;
    }

    bool VisitClassTemplateDecl(ClassTemplateDecl *D);
    //bool VisitFunctionTemplateDecl(FunctionTemplateDecl *D);
    bool VisitTemplateTypeParmTypeLoc(TemplateTypeParmTypeLoc Loc);

private:
    SubstituteClassTemplateParameter *ConsumerInstance;
};

//class SubstituteClassTemplateParameterRewriteVisitor : public
//   RecursiveASTVisitor<SubstituteClassTemplateParameterRewriteVisitor> {
//public:
//    explicit SubstituteClassTemplateParameterRewriteVisitor(
//            SubstituteClassTemplateParameter *Instance)
//        : ConsumerInstance(Instance)
//    {}
//
//    bool TraverseTemplateArgumentLoc(const TemplateArgumentLoc& argloc) {
//        return true;
//    }
//
//    bool VisitTemplateTypeParmTypeLoc(TemplateTypeParmTypeLoc Loc);
//    bool VisitDeclRefExpr(DeclRefExpr *Expr) {
//        printf("DeclRefExpr=\n");
//        Expr->dump();
//        return true;
//    }
//
//private:
//    SubstituteClassTemplateParameter *ConsumerInstance;
//};
//
//bool SubstituteClassTemplateParameterRewriteVisitor::VisitTemplateTypeParmTypeLoc(TemplateTypeParmTypeLoc Loc)
//{
//    //printf("VisitTemplateTypeParmTypeLoc\n");
//    const TemplateTypeParmType *Ty =
//        dyn_cast<TemplateTypeParmType>(Loc.getTypePtr());
//    TransAssert(Ty && "Invalid TemplateSpecializationType!");
//
//    if (Ty->getIndex() == ConsumerInstance->TheParameterIndex) {
//        PrintingPolicy Policy = ConsumerInstance->TheClassTemplateDecl->getASTContext().getPrintingPolicy();
//        std::string argStr;
//        raw_string_ostream out(argStr);
//        ConsumerInstance->TheTemplateArgument->print(Policy, out);
//        SourceRange Range = Loc.getSourceRange();
//        ConsumerInstance->TheRewriter.ReplaceText(Range, out.str());
//        ConsumerInstance->madeTransformation = true;
//    }
//    return true;
//}

template<typename T>
static const TemplateArgument* ArgForTemplateParam(T *D, unsigned paramIdx)
{
    const TemplateArgument *firstArg = 0;
    for (typename T::spec_iterator I = D->spec_begin(), E = D->spec_end();
            I != E; ++I) {
        const TemplateArgument &curArg = (*I)->getTemplateArgs()[paramIdx];
        if (!firstArg)
            firstArg = &curArg;
        else if (!curArg.structurallyEquals(*firstArg))
            return 0;
    }
    return firstArg;
}

template<typename T>
void SubstituteClassTemplateParameter::SaveValidTemplateArguments(T *D)
{
    T *CanonicalD = D->getCanonicalDecl();
    if (VisitedTemplateDecls.count(CanonicalD))
      return;

    VisitedTemplateDecls.insert(CanonicalD);

    const TemplateParameterList *TPList = CanonicalD->getTemplateParameters();
    unsigned Index;
    for (Index = 0; Index < TPList->size(); ++Index) {
        const TemplateArgument *arg = ArgForTemplateParam(CanonicalD, Index);
        if (!arg)// || arg->getKind() != TemplateArgument::Type)
            continue;
        ValidArguments[TPList->getParam(Index)] = arg;
        // ???
//        for (typename T::spec_iterator I = D->spec_begin(), E = D->spec_end();
//                I != E; ++I) {
//            if ((*I)->classofKind(Decl::ClassTemplatePartialSpecialization)) {
//                ClassTemplatePartialSpecializationDecl *PSD = static_cast<ClassTemplatePartialSpecializationDecl*>(*I);
//                ValidArguments[PSD->getTemplateParameters()->getParam(Index)] = arg;
//            }
//        }
        fprintf(stderr, "Added ValidArgument '");
        std::cerr << ArgumentToString(arg, CanonicalD->getASTContext()) << "'\n";
    }
}

bool SubstituteClassTemplateParameterASTVisitor::VisitTemplateTypeParmTypeLoc(TemplateTypeParmTypeLoc Loc)
{
    const TemplateTypeParmType *Ty =
        dyn_cast<TemplateTypeParmType>(Loc.getTypePtr());
    TransAssert(Ty && "Invalid TemplateTypeParmType!");

    fprintf(stderr, "Ty = %d Index = %d\n", Ty, Ty->getIndex());
    Ty->dump();

    TemplateTypeParmDecl *D = Ty->getDecl();
    if (ConsumerInstance->ValidArguments.count(D)) {
        const TemplateArgument *arg = ConsumerInstance->ValidArguments[D];
        ConsumerInstance->ValidInstanceNum++;
        if (ConsumerInstance->ValidInstanceNum ==
                ConsumerInstance->TransformationCounter) {
            ConsumerInstance->TheTemplateArgument = arg;
            ConsumerInstance->TheSourceRange = new SourceRange(Loc.getSourceRange());
        }
    }
    else
        fprintf(stderr, "Bad\n");
    return true;
}

bool SubstituteClassTemplateParameterASTVisitor::VisitClassTemplateDecl(ClassTemplateDecl *D)
{
    ConsumerInstance->SaveValidTemplateArguments(D);
    return true;
}

//bool SubstituteClassTemplateParameterASTVisitor::VisitFunctionTemplateDecl(FunctionTemplateDecl *D)
//{
//    ConsumerInstance->SaveValidTemplateArguments(D);
//    return true;
//}

void SubstituteClassTemplateParameter::Initialize(ASTContext &context)
{
    Transformation::Initialize(context);
    CollectionVisitor = new SubstituteClassTemplateParameterASTVisitor(this);
    //RewriteVisitor = new SubstituteClassTemplateParameterRewriteVisitor(this);
}

void SubstituteClassTemplateParameter::HandleTranslationUnit(ASTContext &Ctx)
{
    if (TransformationManager::isCLangOpt()) {
      ValidInstanceNum = 0;
    }
    else {
      CollectionVisitor->TraverseDecl(Ctx.getTranslationUnitDecl());
    }

    if (QueryInstanceOnly)
      return;

    printf("ValidInstanceNum=%d \n", ValidInstanceNum);
    if (TransformationCounter > ValidInstanceNum) {
      TransError = TransMaxInstanceError;
      return;
    }

    TransAssert(TheClassTemplateDecl && "NULL TheClassTemplateDecl!");
    TransAssert(TheSourceRange);
//    TransAssert(RewriteVisitor && "NULL RewriteVisitor!");
    Ctx.getDiagnostics().setSuppressAllDiagnostics(false);

    TheRewriter.ReplaceText(*TheSourceRange, ArgumentToString(TheTemplateArgument, Ctx));

    //RewriteVisitor->TraverseDecl(Ctx.getTranslationUnitDecl());

    if (Ctx.getDiagnostics().hasErrorOccurred() ||
        Ctx.getDiagnostics().hasFatalErrorOccurred())
      TransError = TransInternalError;
}

