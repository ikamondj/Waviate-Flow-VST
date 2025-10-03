/*
  ==============================================================================

    Runner.cpp
    Created: 15 Aug 2025 10:46:29pm
    Author:  ikamo

  ==============================================================================
*/
#define NOMINMAX
#define USE_GRAPH_EXEC
#define USE_EMBEDDED_CLANG
#include <JuceHeader.h>
#ifdef _WIN32
#include <windows.h>
#endif
#include <shlobj.h>
#include <string>
#include <iostream>
#include <fstream>

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/Support/TargetSelect.h>
#include <clang/CodeGen/CodeGenAction.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Tooling/Tooling.h>
#include <clang/Frontend/FrontendActions.h>
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/OptimizationLevel.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include <llvm/Support/VirtualFileSystem.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/raw_ostream.h>

#ifdef _WIN32
std::string triple = "x86_64-pc-windows-msvc";
#elif __APPLE__
std::string triple = "x86_64-apple-darwin";
#else
std::string triple = "x86_64-unknown-linux-gnu";
#endif



#include "Runner.h"
#include "NodeData.h"
#include "SceneComponent.h"
#include "UserInput.h"
#include "NodeComponent.h"
#include "PluginProcessor.h"
#include <format>
#include "NodeType.h"


void Runner::setupIterative(NodeData* root, RunnerInput& inlineInstance) {
	if (!root) return;

	struct Frame {
		NodeData* node;
		int inputIndex;
	};

	std::vector<Frame> stack;
	stack.push_back({ root, 0 });

	while (!stack.empty()) {
		Frame& frame = stack.back();
		NodeData* node = frame.node;

		// Already processed?
		if (!node || inlineInstance.safeOwnership.contains(node)) {
			stack.pop_back();
			continue;
		}

		// Process inputs one by one (simulate recursion)
		if (frame.inputIndex < node->getNumInputs()) {
			auto input = node->getInput(frame.inputIndex++);
			if (input != nullptr && !inlineInstance.safeOwnership.contains(input)) {
				stack.push_back({ input, 0 });
			}
		}
		else {
			// All inputs done, now "return" and process node
			int size = node->getCompileTimeSize(&inlineInstance);
			int offset = inlineInstance.field.size();
			inlineInstance.field.resize(inlineInstance.field.size() + size);
			inlineInstance.safeOwnership.insert({ node, {offset, size} });
			inlineInstance.nodesOrder.push_back(node);

			stack.pop_back();
		}
	}
}


static std::unique_ptr<llvm::orc::LLJIT> GlobalJIT;  // keep alive

#ifdef _WIN32
#include <windows.h>
#define DYNLIB_HANDLE HMODULE
#define DYNLIB_LOAD(name) LoadLibraryA(name)
#define DYNLIB_GETSYM GetProcAddress
#define DYNLIB_CLOSE FreeLibrary
#define DLL_EXT ".dll"
#elif __APPLE__
#include <dlfcn.h>
#define DYNLIB_HANDLE void*
#define DYNLIB_LOAD(name) dlopen(name, RTLD_NOW)
#define DYNLIB_GETSYM dlsym
#define DYNLIB_CLOSE dlclose
#define DLL_EXT ".dylib"
#else
#include <dlfcn.h>
#define DYNLIB_HANDLE void*
#define DYNLIB_LOAD(name) dlopen(name, RTLD_NOW)
#define DYNLIB_GETSYM dlsym
#define DYNLIB_CLOSE dlclose
#define DLL_EXT ".so"
#endif

std::string optFlag(OptLevel opt) {
#ifdef _WIN32
	switch (opt) {
	case OptLevel::minimal: return "/Od";
	case OptLevel::low: return "/O1";
	case OptLevel::medium: return "/O2";
	case OptLevel::high: return "/Ox";  // MSVC's "full optimize"
	case OptLevel::prioritizeSize: return "/O1";  // size-ish
	case OptLevel::tradeOffSize: return "/O1";  // closest equivalent
	}
#else
	switch (opt) {
	case OptLevel::O0: return "-O0";
	case OptLevel::O1: return "-O1";
	case OptLevel::O2: return "-O2";
	case OptLevel::O3: return "-O3";
	case OptLevel::Os: return "-Os";
	case OptLevel::Oz: return "-Oz";
	}
#endif
	return "";
}

#include <array>
#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <sstream>
#include <iostream>
#include <fstream>

NodeFn compileNodeKernelDll(const std::string& sourceCode,
	const std::string& funcName = "nodeTypeOutput",
	OptLevel opt = OptLevel::high)
{
	// 1. Write source to a temporary file
	std::string srcFile = "jit_input.c";
	std::ofstream out(srcFile);
	out << sourceCode;
	out.close();

	// 2. Decide output lib name
	static std::atomic<uint64_t> dllCounter{ 0 };
	std::string libFile = "jit_output_" + std::to_string(dllCounter++) + DLL_EXT;


	// 3. Build compile command using clang
	std::string cmd;
#if defined(_WIN32)
	// Windows: build DLL with clang, link dynamically
	cmd = "clang -shared -o " + libFile + " " + srcFile + " -O2 2>&1";
#elif defined(__APPLE__)
	// macOS: .dylib
	cmd = "clang -dynamiclib -o " + libFile + " " + srcFile + " -O2 2>&1";
#else
	// Linux: .so
	cmd = "clang -shared -fPIC -o " + libFile + " " + srcFile + " -O2 2>&1";
#endif

	std::cout << "[compileNodeKernelDll] Running command:\n" << cmd << std::endl;

	// 4. Run command and capture stdout+stderr
	std::array<char, 128> buffer{};
	std::string result;
#ifdef _WIN32
	FILE* pipe = _popen(cmd.c_str(), "r");
#else
	FILE* pipe = popen(cmd.c_str(), "r");
#endif
	if (!pipe) {
		throw std::runtime_error("popen() failed when invoking compiler");
	}
	while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
		result += buffer.data();
	}
#ifdef _WIN32
	int ret = _pclose(pipe);
#else
	int ret = pclose(pipe);
#endif

	// 5. Check return code
	if (ret != 0) {
		std::ostringstream oss;
		oss << "Failed to compile dynamic library.\n"
			<< "Command: " << cmd << "\n"
			<< "Exit code: " << ret << "\n"
			<< "Compiler output:\n" << result;
		throw std::runtime_error(oss.str());
	}

	std::cout << "[compileNodeKernelDll] Compiler output:\n" << result << std::endl;

	// 6. Load dynamic library
	DYNLIB_HANDLE handle = DYNLIB_LOAD(libFile.c_str());
	if (!handle) {
		throw std::runtime_error("Failed to load dynamic library: " + libFile);
	}

	// 7. Resolve symbol
	void* sym = (void*)DYNLIB_GETSYM(handle, funcName.c_str());
	if (!sym) {
		DYNLIB_CLOSE(handle);
		throw std::runtime_error("Failed to resolve symbol: " + funcName);
	}

	return reinterpret_cast<NodeFn>(sym);
}



NodeFn compileNodeKernel(const std::string& sourceCode,
	const std::string& funcName = "nodeTypeOutput",
	llvm::OptimizationLevel OX = llvm::OptimizationLevel::O3)
{
	using namespace llvm;
	using namespace llvm::orc;

	static bool initialized = false;
	if (!initialized) {
		InitializeNativeTarget();
		InitializeNativeTargetAsmPrinter();
		InitializeNativeTargetAsmParser();
		initialized = true;
	}

	// --- 1. Single LLVM context
	auto TSCtx = std::make_unique<LLVMContext>();
	LLVMContext* Ctx = TSCtx.get();

	// --- 2. Setup diagnostics
	auto diagOpts = std::make_shared<clang::DiagnosticOptions>();
	auto diagPrinter = std::make_unique<clang::TextDiagnosticPrinter>(
		llvm::errs(), &*diagOpts);
	clang::IntrusiveRefCntPtr<clang::DiagnosticIDs> diagID(new clang::DiagnosticIDs());
	clang::DiagnosticsEngine diags(diagID, &*diagOpts, diagPrinter.get(), false);

	// --- 3. CompilerInvocation
	auto invocation = std::make_shared<clang::CompilerInvocation>();
	clang::CompilerInvocation::CreateFromArgs(
		*invocation,
		{ "-xc", "-std=c99", "-Wall", "-Wextra", "-O2" },
		diags);

	// --- 4. CompilerInstance
	clang::CompilerInstance compiler;
	compiler.setInvocation(invocation);
	compiler.createDiagnostics(diagPrinter.release(), true);

	auto TO = std::make_shared<clang::TargetOptions>();
	TO->Triple = triple;
	compiler.setTarget(clang::TargetInfo::CreateTargetInfo(compiler.getDiagnostics(), TO));

	compiler.createFileManager();
	compiler.createSourceManager(compiler.getFileManager());

	// --- 5. Feed in-memory file
	auto buffer = llvm::MemoryBuffer::getMemBufferCopy(sourceCode, "jit_input.c");
	compiler.getSourceManager().setMainFileID(
		compiler.getSourceManager().createFileID(std::move(buffer)));

	// --- 6. Run EmitLLVMOnlyAction
	clang::EmitLLVMOnlyAction action(Ctx);
	if (!compiler.ExecuteAction(action)) {
		throw std::runtime_error("clang failed to compile to LLVM IR");
	}

	std::unique_ptr<Module> M = action.takeModule();
	if (!M) {
		throw std::runtime_error("EmitLLVMOnlyAction returned no module");
	}

	// --- 7. Optimize the module
	llvm::LoopAnalysisManager LAM;
	llvm::FunctionAnalysisManager FAM;
	llvm::CGSCCAnalysisManager CGAM;
	llvm::ModuleAnalysisManager MAM;

	llvm::PassBuilder PB;
	PB.registerModuleAnalyses(MAM);
	PB.registerCGSCCAnalyses(CGAM);
	PB.registerFunctionAnalyses(FAM);
	PB.registerLoopAnalyses(LAM);
	PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

	llvm::ModulePassManager MPM = PB.buildPerModuleDefaultPipeline(OX);
	MPM.run(*M, MAM);

	// --- 8. Initialize the JIT if needed
	if (!GlobalJIT) {
		GlobalJIT = cantFail(LLJITBuilder().create());
	}

	// --- 9. Hand module to ORC
	orc::ThreadSafeModule TSM(std::move(M), std::move(TSCtx));
	cantFail(GlobalJIT->addIRModule(std::move(TSM)));

	// --- 10. Lookup symbol
	auto sym = cantFail(GlobalJIT->lookup(funcName));
	return sym.toPtr<NodeFn>();
}




uint64_t globalCompileCounter;

void Runner::setupRecursive(NodeData* node, RunnerInput& inlineInstance) {
	
	if (!node || inlineInstance.safeOwnership.contains(node)) return;
	std::vector < std::tuple<int, int> > tempSpans;
	for (int i = 0; i < node->getNumInputs(); i += 1) {
		auto input = node->getInput(i);
		if (input != nullptr) {
			setupRecursive(input, inlineInstance);
			auto [start,length] = inlineInstance.safeOwnership[input];
			tempSpans.push_back({start, length});
		}
	}
	int size = node->getCompileTimeSize(&inlineInstance);
	int offset = inlineInstance.field.size();
	inlineInstance.field.resize(inlineInstance.field.size() + size);
	inlineInstance.safeOwnership.insert({ node, {offset, size} });
	inlineInstance.nodesOrder.push_back(node);
}

inline void convert(const std::span<ddtype>& data, InputType outboundType, InputType inboundType) {
	if (outboundType == InputType::decimal) {
		if (inboundType == InputType::boolean) {
			for (ddtype& d : data) { d.i = d.d > 0.5 ? 1 : 0; }
		}
		else if (inboundType == InputType::integer) {
			for (ddtype& d : data) { d.i = static_cast<int64_t>(std::round(d.d)); }
		}
	}
	else if (outboundType == InputType::integer) {
		if (inboundType == InputType::boolean) {
			for (ddtype& d : data) { d.i = (d.i == 0 ? 0 : 1); }
		}
		else if (inboundType == InputType::decimal) {
			for (ddtype& d : data) { d.d = static_cast<double>(d.i); }
		}
	}
	else if (inboundType == InputType::decimal) {
		for (ddtype& d : data) { d.d = d.i ? 1.0 : 0.0; }
	}
}

std::span<ddtype> Runner::runClang(RunnerInput* runnerInputP, UserInput& userInput, const std::vector<std::span<ddtype>>& outerInputs)
{
	int outputSize = runnerInputP->outputNode->getCompileTimeSize(runnerInputP);
	std::vector<ddtype> result;
	result.resize(outputSize);

	std::vector<ddtype*> inputPtrs;
	std::vector<int> inputSizes;

	inputPtrs.reserve(outerInputs.size());
	inputSizes.reserve(outerInputs.size());

	for (const auto& sp : outerInputs) {
		inputPtrs.push_back(sp.data());               // pointer to ddtype
		inputSizes.push_back(static_cast<int>(sp.size())); // length
	}

	runnerInputP->compiledFunc(runnerInputP->field.data(), runnerInputP->field.size(), result.data(), outputSize, inputPtrs.data(), inputSizes.data(), static_cast<int>(inputPtrs.size()), &userInput);

	return std::span<ddtype>(result);
}

std::span<ddtype> Runner::run(const RunnerInput* runnerInputP, UserInput& userInput, const std::vector<std::span<ddtype>>& outerInputs)
{
	if (!runnerInputP) return std::span<ddtype, 0>();
	auto& runnerInput = *runnerInputP;
	if (runnerInput.nodeCopies.empty()) return std::span<ddtype, 0>();
	for (NodeData* node : runnerInput.nodesOrder)
	{
		auto& output = runnerInput.nodeOwnership.at(node);
		std::vector<std::span<ddtype>> inputs;
		std::vector<ddtype> emptyInputDefaults(node->getNumInputs());
		for (int i = 0; i < node->getNumInputs(); i += 1) {
			if (auto inputNode = node->getInput(i)) {
				auto& otherspan = runnerInput.nodeOwnership.at(inputNode);
				auto outboundType = inputNode->getType()->outputType;
				auto inboundType = node->getType()->inputs[i].inputType;
				if (outboundType == InputType::followsInput) {
					outboundType = inputNode->getTrueType();
				}
				if (inboundType == InputType::any) {
					inboundType = node->getTrueType();
				}
				convert(otherspan, outboundType, inboundType);
				inputs.push_back(otherspan);
			}
			else {
				emptyInputDefaults[i] = node->defaultValues[i];
				std::span<ddtype> otherspan(&emptyInputDefaults[i], 1);
				inputs.push_back(otherspan);
			}
		}
		ddtype defaultIfNeeded = 0.0;
		
		
		if (node->getType()->isInputNode) {
			int inputIndex = node->inputIndex;
			if (inputIndex >= 0 && inputIndex < outerInputs.size()) {
				inputs.push_back(outerInputs[inputIndex]);
			}
			else {
				if (node->getType()->outputType == InputType::decimal) {
					defaultIfNeeded.d = node->getNumericProperties().contains("defaultValue") ? node->getNumericProperty("defaultValue") : 0.0;
				}
				else if (node->getType()->outputType == InputType::boolean) {
					defaultIfNeeded.i = node->getNumericProperties().contains("defaultValue") ? (node->getNumericProperty("defaultValue") > 0.5 ? 1 : 0) : 0;
				}
				else {
					defaultIfNeeded.i = node->getNumericProperties().contains("defaultValue") ? static_cast<int>(std::round(node->getNumericProperty("defaultValue"))) : 0;
				}
				std::span<ddtype> spanOverDefaultIfNeeded(&defaultIfNeeded, 1);
				inputs.push_back(spanOverDefaultIfNeeded);
			}
		}
		node->getType()->execute(*node, userInput, inputs, output, runnerInput);
	}
	return runnerInput.nodeOwnership.at(runnerInput.outputNode);
}


std::span<ddtype> Runner::getNodeField(NodeData* nodeData, std::unordered_map<NodeData*, std::span<ddtype>>& nodeOwnership)
{
	return nodeOwnership[nodeData];
}

bool Runner::containsNodeField(NodeData* d, std::unordered_map<NodeData*, std::span<ddtype>>& nodeOwnership)
{
	return nodeOwnership.contains(d);
}

		
std::vector<ddtype> Runner::findRemainingSizes(
	NodeData* root,
	RunnerInput& inlineInstance,
	const std::vector<std::span<ddtype>>& outerInputs, UserInput& user)
{
	// If already cached
	if (inlineInstance.nodeCompileTimeOutputs.contains(root)) {
		return inlineInstance.nodeCompileTimeOutputs[root];
	}

	struct Frame {
		NodeData* node;
		int state; // 0 = expand inputs, 1 = compute
	};

	std::vector<ddtype> result;
	std::stack<Frame> stack;
	stack.push({ root, 0 });

	while (!stack.empty()) {
		Frame frame = stack.top();
		stack.pop();

		NodeData* node = frame.node;

		// If cached, reuse and continue
		if (inlineInstance.nodeCompileTimeOutputs.contains(node)) {
			continue;
		}

		if (frame.state == 0) {
			// Expand inputs first
			frame.state = 1;
			stack.push(frame); // push back current frame for later compute

			for (int i = node->getNumInputs() - 1; i >= 0; i--) {
				NodeData* input = node->getInput(i);
				if (input && !inlineInstance.nodeCompileTimeOutputs.contains(input)) {
					stack.push({ input, 0 }); // process child first
				}
			}
		}
		else {
			// All inputs processed → compute this node
			std::vector<std::vector<ddtype>> inputspans;
			inputspans.reserve(node->getNumInputs() + (node->getType()->isInputNode ? 1 : 0));

			for (int i = 0; i < node->getNumInputs(); i++) {
				NodeData* input = node->getInput(i);
				if (input) {
					inputspans.push_back(inlineInstance.nodeCompileTimeOutputs[input]);
				}
				else {
					inputspans.push_back({ node->defaultValues[i] });
				}
			}

			if (node->getType()->isInputNode) {
				int inputIndex = node->inputIndex;
				if (outerInputs.empty()) {
					inputspans.push_back({ 0.0 }); // TODO: custom defaults
				}
				else {
					inputspans.push_back(std::vector<ddtype>(
						outerInputs[inputIndex].begin(),
						outerInputs[inputIndex].end()));
				}
			}

			std::vector<std::span<ddtype>> actualspans;
			actualspans.reserve(inputspans.size());
			for (auto& vec : inputspans) {
				actualspans.emplace_back(vec);
			}

			int dim = node->getMaxOutputDimension(inputspans, inlineInstance, node->inputIndex);
			node->setCompileTimeSize(&inlineInstance, dim);

			std::vector<ddtype> tempOutput(dim, 0.0);
			std::span<ddtype> tempOutSpan(tempOutput);

			
			node->getType()->execute(*node, user, actualspans, tempOutSpan, inlineInstance);

			inlineInstance.nodeCompileTimeOutputs[node] = tempOutput;

			if (node == root) {
				result = tempOutput;
			}
		}
	}

	return result;
}



void storeCopies(RunnerInput& input,
	SceneData* startScene,
	NodeData*& editorOutput,
	std::unordered_map<NodeData*, NodeData*>& remap)
{
		std::vector<NodeData*> sceneInputNodes;

		// clone nodes
		for (int i = 0; i < (int)startScene->nodeDatas.size(); i++) {
			auto& n = *startScene->nodeDatas[i];
			input.nodeCopies.push_back(std::make_unique<NodeData>(n)); // deep copy
			NodeData* copyPtr = input.nodeCopies.back().get();
			if (copyPtr) {
				remap[&n] = copyPtr;
				remap[copyPtr] = &n;

				if (n.getType()->name == "output") {
					if (!editorOutput) {
						editorOutput = copyPtr; // safe, only if copy exists
					}
				}
			}
			
		}


	// cleanup / fix references
	for (auto& nodeCopy : input.nodeCopies) {
		if (!nodeCopy) continue;

		// fix inputs
		for (int i = 0; i < nodeCopy->getNumInputs(); i++) {
			if (auto in = nodeCopy->getInput(i)) {
				if (!in->isCopy) {
					auto it = remap.find(in);
					if (it != remap.end()) {
						nodeCopy->detachInput(i, nullptr);
						nodeCopy->attachInput(i, it->second, input, nullptr);
					}
				}
			}
		}

		// fix outputs
		for (const auto& [output, idx] : nodeCopy->outputs) {
			if (output && !output->isCopy) {
				auto it = remap.find(output);
				if (it != remap.end()) {
					it->second->attachInput(idx, nodeCopy.get(), input, nullptr);
				}
			}
		}
	}
}

// Sanitizer for numeric values (ensures finite literal, not NaN/inf)
inline std::string emitNumericLiteral(double d) {
	if (!std::isfinite(d)) {
		return "0.0"; // fallback
	}
	char buf[64];
	std::snprintf(buf, sizeof(buf), "%.17g", d); // precise, portable C99 literal
	return buf;
}

// Sanitizer for string values (UI ensures, but double safety here)
inline std::string sanitizeIdentifier(const std::string& s) {
	std::string out;
	out.reserve(s.size());
	for (char c : s) {
		if ((c >= 'a' && c <= 'z') ||
			(c >= 'A' && c <= 'Z') ||
			(c >= '0' && c <= '9') ||
			c == '_') {
			out.push_back(c);
		}
	}
	if (out.empty() || std::isdigit(out[0])) {
		out = "_" + out; // must be valid identifier
	}
	return out;
}



std::string Runner::initializeClang(const RunnerInput& input,
	const SceneData* scene,
	const std::vector<std::span<ddtype>>& /*outerInputs*/)
{
	std::string emitCode;
	std::unordered_map<NodeData*, int> nodeIndex;   // node -> ordinal
	std::unordered_map<NodeData*, int> nodeCounts;  // node -> element count
	std::unordered_map<NodeData*, int> nodeOffset;  // node -> start offset in dataField
	std::unordered_map<std::string, GlobalClangVar> varDeclarations;

	// 1) Assign indices and record sizes
	int idx = 0;
	int totalElems = 0;
	for (auto* nd : input.nodesOrder) {
		const int count = nd->getCompileTimeSize(&input);
		nodeIndex[nd] = idx;
		nodeCounts[nd] = count;
		nodeOffset[nd] = totalElems;      // start of this node’s slice
		totalElems += count;
		++idx;

		for (auto& gv : nd->getType()->globalVarNames(*nd, nodeIndex[nd])) {
			std::string sanitized = sanitizeIdentifier(gv.varName);
			if (!varDeclarations.count(sanitized)) {
				auto safe = gv; safe.varName = sanitized;
				varDeclarations.emplace(sanitized, safe);
			}
		}
	}

	// 2) Emit total requirement and optional guard
	emitCode += "const int kTotalArenaElems = " + std::to_string(totalElems) + ";\n";
	emitCode += "(void)kTotalArenaElems;\n";
	// If you want a cheap guard (won’t crash if caller passes wrong size)
	emitCode += "if (dataFieldSize < kTotalArenaElems) { /* TODO: handle error */ return; }\n";

	// 3) Emit global vars
	for (const auto& [_, gv] : varDeclarations) {
		emitCode += (gv.isStatic ? "static " : "") + gv.type + " " + gv.varName + ";\n";
	}

	// 4) Emit per-node code blocks
	int ord = 0;
	for (auto* nd : input.nodesOrder) {
		const int i = nodeIndex.at(nd);
		const int count = nodeCounts.at(nd);
		const int startOff = nodeOffset.at(nd);

		emitCode += "{\n";

		// Arena-backed outputs for non-final nodes
		if (nd != input.outputNode) {
			emitCode += "  ddtype* o = dataField + " + std::to_string(startOff) + ";\n";
			emitCode += "  int osize = " + std::to_string(count) + ";\n";
		}
		else {
			// Final node writes to provided output buffer
			emitCode += "  ddtype* o = output;\n";
			emitCode += "  int osize = outputSize;\n";
		}

		// Numeric props
		for (auto& [k, v] : nd->getNumericProperties()) {
			std::string sk = sanitizeIdentifier(k);
			emitCode += "  double n_" + sk + " = " + emitNumericLiteral(v) + ";\n";
		}

		// String props
		for (auto& [k, v] : nd->getProperties()) {
			emitCode += "  const char* s_" + k + " = \"" + v + "\";\n";
		}

		// Optional stored audio → local array (C99)
		if (!nd->optionalStoredAudio.empty()) {
			emitCode += "  ddtype stores[" + std::to_string(nd->optionalStoredAudio.size()) + "];\n";
			for (size_t k = 0; k < nd->optionalStoredAudio.size(); ++k) {
				emitCode += "  stores[" + std::to_string(k) + "].i = "
					+ std::to_string(nd->optionalStoredAudio[k].i) + "LL;\n";
			}
		}

		// Inputs
		if (nd->getType()->isInputNode) {
			const int pin = nd->inputIndex;
			emitCode += "  ddtype* i0 = inputs[" + std::to_string(pin) + "];\n";
			emitCode += "  int     isize0 = inputSizes[" + std::to_string(pin) + "];\n";
		}
		else {
			for (int j = 0; j < nd->getNumInputs(); ++j) {
				auto* src = nd->getInput(j);
				if (src) {
					const int sIdx = nodeIndex.at(src);
					const int sCount = nodeCounts.at(src);
					const int sOff = nodeOffset.at(src);
					emitCode += "  ddtype* i" + std::to_string(j)
						+ " = dataField + " + std::to_string(sOff) + ";\n";
					emitCode += "  int     isize" + std::to_string(j)
						+ " = " + std::to_string(sCount) + ";\n";

					// (Optional) type-conversion buffer if required
					// Emit under a condition the same way you did before:
					//   ddtype* iJ_conv = (ddtype*)malloc(isizeJ * sizeof(ddtype));
					//   for (...) iJ_conv[k] = convert_ddtype(...);
					//   iJ = iJ_conv;
				}
				else {
					// Unconnected input → use stack-allocated ddtype
					auto type = nd->getType()->inputs[j].inputType;

					// Define a local variable
					emitCode += "  ddtype i" + std::to_string(j) + "_default;\n";

					// Initialize it
					if (type == InputType::decimal) {
						emitCode += "  i" + std::to_string(j) + "_default.d = "
							+ std::to_string(nd->defaultValues[j].d) + ";\n";
					}
					else {
						emitCode += "  i" + std::to_string(j) + "_default.i = "
							+ std::to_string(nd->defaultValues[j].i) + ";\n";
					}

					// Pointer to the default
					emitCode += "  ddtype* i" + std::to_string(j)
						+ " = &i" + std::to_string(j) + "_default;\n";
					emitCode += "  int isize" + std::to_string(j) + " = 1;\n";
				}
			}
		}

		// Node body
		emitCode += "  " + nd->getType()->emitCode(*nd, ord) + "\n";

		// (Optional) Free any conversion/default buffers you actually allocated above
		// emitCode += "  /* free(iJ_conv / iJ defaults if allocated) */\n";

		emitCode += "}\n\n";
		++ord;
	}

	return emitCode;
}



const char* ddtypeClang =
"#include <stdint.h>\n"
"#include <math.h>\n"
"#include <stdlib.h>\n"
"#include <stdbool.h>\n"
"typedef union {\n"
"    double d;\n"
"    int64_t i;\n"
"} ddtype;\n";

const juce::String ddtypeClangJ(ddtypeClang);

const juce::String clangHeader(uint64_t x) {
	return ddtypeClangJ + UserInputClangJ +
#ifdef _WIN32
		"__declspec(dllexport) " +
#endif
		"void nodeTypeOutput" + juce::String(x) +
		"(ddtype* dataField, int dataFieldSize, "
		"ddtype* output, int outputSize, "
		"ddtype** inputs, int* inputSizes, int numInputs) { ";
}

const juce::String clangCloser = "}";

void Runner::initialize(RunnerInput& input, class SceneData* scene,
	const std::vector<std::span<ddtype>>& outerInputs)
{
	input.nodesOrder.clear();
	input.nodeOwnership.clear();
	input.safeOwnership.clear();
	input.nodeCompileTimeOutputs.clear();
	input.nodeCopies.clear();
	input.remap.clear();
	input.field.clear();
	input.clangcode = "";
	if (!scene) return;
	if (scene->nodeDatas.empty()) return;

	std::unordered_map<NodeData*, NodeData*>& remap = input.remap;
	NodeData* editorOutput = nullptr;

	storeCopies(input, scene, editorOutput, remap);

	// fallback if index 0 should be output
	if (!editorOutput) editorOutput = input.nodeCopies[0].get();
	input.outputNode = editorOutput;

	for (auto& newNode : input.nodeCopies)
		newNode->markUncompiled(&input);

	std::unique_ptr<UserInput> userInput = std::make_unique<UserInput>();
	for (auto& node : input.nodeCopies)
		findRemainingSizes(node.get(), input, outerInputs, *userInput); // just warms the cache

	setupIterative(input.outputNode, input);

	for (auto& [node, ownership] : input.safeOwnership) {
		if (node) {
			auto [offset, size] = ownership;
			input.nodeOwnership[node] = std::span<ddtype>(input.field.data() + offset, size);
		}
	}

	juce::String clangCode = initializeClang(input, scene, outerInputs);
	input.clangcode = (clangHeader(globalCompileCounter) + clangCode + clangCloser).toStdString();

	
#ifndef USE_GRAPH_EXEC
	llvm::OptimizationLevel OX;
	switch (input.optLevel) {
	case OptLevel::high:           OX = llvm::OptimizationLevel::O3; break;
	case OptLevel::medium:         OX = llvm::OptimizationLevel::O2; break;
	case OptLevel::low:            OX = llvm::OptimizationLevel::O1; break;
	case OptLevel::minimal:        OX = llvm::OptimizationLevel::O0; break;
	case OptLevel::prioritizeSize: OX = llvm::OptimizationLevel::Oz; break;
	case OptLevel::tradeOffSize:   OX = llvm::OptimizationLevel::Os; break;
	}
#ifdef USE_EMBEDDED_CLANG
	input.compiledFunc = compileNodeKernel(
		input.clangcode, (juce::String("nodeTypeOutput") + juce::String(globalCompileCounter)).toStdString(), OX);
#else 
	input.compiledFunc = compileNodeKernelDll(
		input.clangcode,
		(juce::String("nodeTypeOutput") + juce::String(globalCompileCounter)).toStdString(), input.optLevel);
		//,OX);
#endif
#endif

	// Handle compile-time known nodes
	std::vector<NodeData*> tempNodesOrder;
	for (NodeData* node : input.nodesOrder) {
		if (node->isCompileTimeKnown()) {
			input.compileTimeKnown.insert(node);

			std::vector<ddtype> extraspace(node->getNumInputs());
			for (int i = 0; i < node->getNumInputs(); ++i)
				extraspace[i] = node->defaultValues[i];

			auto& output = input.nodeOwnership[node];
			std::vector<std::span<ddtype>> inputs;
			for (int i = 0; i < node->getNumInputs(); ++i) {
				if (auto* nodeInput = node->getInput(i))
					inputs.push_back(input.nodeOwnership[nodeInput]);
				else
					inputs.push_back(std::span<ddtype>(&extraspace[i], 1));
			}

			ddtype defaultIfNeeded = node->getNumericProperties().contains("defaultValue")
				? node->getNumericProperty("defaultValue") : 0.0;
			std::span<ddtype> spanOverDefaultIfNeeded(&defaultIfNeeded, 1);

			if (node->getType()->isInputNode) {
				int inputIndex = node->inputIndex;
				if (inputIndex >= 0 && inputIndex < (int)outerInputs.size())
					inputs.push_back(outerInputs[inputIndex]);
				else
					inputs.push_back(spanOverDefaultIfNeeded);
			}

			node->getType()->execute(*node, *userInput, inputs, output, input);
		}
		else {
			tempNodesOrder.push_back(node);
		}
	}

	for (auto& node : input.nodeCopies) {
		if (remap.contains(node.get())) {
			auto sceneNode = remap[node.get()];
			if (sceneNode)
				sceneNode->setCompileTimeSize(scene, node->getCompileTimeSize(&input));
		}
	}

	input.nodesOrder = tempNodesOrder;
}

