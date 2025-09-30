/*
  ==============================================================================

    Runner.cpp
    Created: 15 Aug 2025 10:46:29pm
    Author:  ikamo

  ==============================================================================
*/
#include <JuceHeader.h>

#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/Support/TargetSelect.h>
#include <clang/CodeGen/CodeGenAction.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/TextDiagnosticPrinter.h>
#include <clang/Tooling/Tooling.h>
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/OptimizationLevel.h"
#include "llvm/Analysis/LoopAnalysisManager.h"



#include "Runner.h"
#include "NodeData.h"
#include "SceneComponent.h"
#include "UserInput.h"
#include "NodeComponent.h"
#include "PluginProcessor.h"
#include <format>
#include "NodeType.h"



static std::unique_ptr<llvm::orc::LLJIT> GlobalJIT;  // keep alive

NodeFn compileNodeKernel(const std::string& sourceCode,
	const std::string& funcName = "nodeTypeOutput", llvm::OptimizationLevel OX = llvm::OptimizationLevel::O3)
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

	// set up Clang to compile C code to LLVM IR
	auto CI = std::make_unique< clang::CompilerInstance>();
	auto diagOpts = llvm::IntrusiveRefCntPtr<clang::DiagnosticOptions>(new clang::DiagnosticOptions());
	auto diagPrinter = new clang::TextDiagnosticPrinter(llvm::errs(), &*diagOpts);
	auto diags = new clang::DiagnosticsEngine(
		llvm::IntrusiveRefCntPtr<clang::DiagnosticIDs>(new clang::DiagnosticIDs()),
		&*diagOpts, diagPrinter);

	CI->createDiagnostics(diagPrinter, false);

	std::vector<const char*> args = {
		"clang++", "-xc++", "-std=c++20"
	};

	auto buffer = llvm::MemoryBuffer::getMemBuffer(sourceCode, "<jit-input>");
	auto action = std::make_unique<clang::EmitLLVMOnlyAction>();
	
	std::unique_ptr<llvm::Module> M = action->takeModule();
	if (!M) throw std::runtime_error("no module");

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

	llvm::ModulePassManager MPM;
	MPM = PB.buildPerModuleDefaultPipeline(OX); // or O2, Os, Oz

	MPM.run(*M, MAM);


	auto jit = cantFail(LLJITBuilder().create());
	auto TSM = orc::ThreadSafeModule(std::move(M), std::make_unique<LLVMContext>());
	cantFail(jit->addIRModule(std::move(TSM)));

	auto sym = cantFail(jit->lookup(funcName));
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

std::span<ddtype> Runner::runClang(const RunnerInput* runnerInputP, UserInput& userInput, const std::vector<std::span<ddtype>>& outerInputs)
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

	runnerInputP->compiledFunc(result.data(), outputSize, inputPtrs.data(), inputSizes.data(), static_cast<int>(inputPtrs.size()), &userInput);

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

		
std::vector<ddtype> Runner::findRemainingSizes(NodeData* root, RunnerInput& inlineInstance, const std::vector<std::span<ddtype>>& outerInputs)
const std::vector<ddtype>& Runner::findRemainingSizes(
	NodeData* node,
	RunnerInput& inlineInstance,
	const std::vector<std::span<ddtype>>& outerInputs)
{
	// If already cached
	if (inlineInstance.nodeCompileTimeOutputs.contains(root)) {
		return inlineInstance.nodeCompileTimeOutputs[root];
	}

	struct Frame {
		NodeData* node;
		int state; // 0 = expand inputs, 1 = compute
		std::vector<std::vector<ddtype>> inputspans;
	};

	std::vector<ddtype> result;
	std::stack<Frame> stack;
	stack.push({ root, 0, {} });

	while (!stack.empty()) {
		Frame frame = std::move(stack.top());
		stack.pop();

		NodeData* node = frame.node;

		// If cached, reuse and continue
		if (inlineInstance.nodeCompileTimeOutputs.contains(node)) {
			continue;
		}

		if (frame.state == 0) {
			// Expand inputs first
			frame.state = 1;
			stack.push(std::move(frame)); // push back current frame for later compute

			for (int i = node->getNumInputs() - 1; i >= 0; i--) {
				NodeData* input = node->getInput(i);
				if (input) {
					if (!inlineInstance.nodeCompileTimeOutputs.contains(input)) {
						stack.push({ input, 0, {} }); // process this child first
					}
				}
			}
		}
		else {
			// All inputs processed � compute this node
			std::vector<std::vector<ddtype>> inputspans;
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
					inputspans.push_back(std::vector<ddtype>(outerInputs[inputIndex].begin(), outerInputs[inputIndex].end()));
				}
			}

			std::vector<std::span<ddtype>> actualspans;
			for (auto& vec : inputspans) {
				actualspans.emplace_back(vec);
			}

			int dim = node->getMaxOutputDimension(inputspans, inlineInstance, node->inputIndex);
			node->setCompileTimeSize(&inlineInstance, dim);

			std::vector<ddtype> tempOutput(dim, 0.0);
			std::span<ddtype> tempOutSpan(tempOutput);

			UserInput user;
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



juce::String Runner::initializeClang(const RunnerInput& input,
	const class SceneData* scene,
	const std::vector<std::span<ddtype>>& outerInputs)
{
	juce::String emitCode;
	std::unordered_map<NodeData*, int> nodeFieldVars;
	std::unordered_map<std::string, GlobalClangVar> varDeclarations;
	int i = 0;

	// Allocate vectors per node
	for (auto nd : input.nodesOrder) {
		int count = nd->getCompileTimeSize(&input);
		nodeFieldVars.insert({ nd, i });

		emitCode << "std::vector<ddtype> field" << juce::String(i)
			<< "(" << juce::String(count) << ");\n";
		emitCode << "int size" << juce::String(i)
			<< " = (int)field" << juce::String(i) << ".size();\n";

		for (auto& gv : nd->getType()->globalVarNames(*nd, i)) {
			std::string sanitized = sanitizeIdentifier(gv.varName);
			if (!varDeclarations.contains(sanitized)) {
				GlobalClangVar safeVar = gv;
				safeVar.varName = sanitized;
				varDeclarations.insert({ sanitized, safeVar });
			}
		}

		++i;
	}

	// Emit global vars
	for (const auto& [_, gv] : varDeclarations) {
		emitCode << (gv.isStatic ? "static " : "") << gv.type << " " << gv.varName << ";\n";
	}

	// Emit code per node
	int ord = 0;
	for (const auto nd : input.nodesOrder) {
		const auto idx = nodeFieldVars.at(nd);
		emitCode << "{\n";

		if (nd != input.outputNode) {
			emitCode << "  std::vector<ddtype>& o = field" << juce::String(idx) << ";\n";
			emitCode << "  int osize = size" << juce::String(idx) << ";\n";
		}
		else {
			emitCode << "  std::vector<ddtype> o(output, output + outputSize);\n";
			emitCode << "  int osize = outputSize;\n";
		}

		// Numeric props
		for (const auto& [c, d] : nd->getNumericProperties()) {
			std::string sanitizedKey = sanitizeIdentifier(c);
			emitCode << "  double n_" << sanitizedKey << " = "
				<< emitNumericLiteral(d) << ";\n";
		}

		// String props
		for (const auto& [k, v] : nd->getProperties()) {
			emitCode << "  const char* s_" << k << " = \"" << v << "\";\n";
		}

		// Optional stored audio
		if (!nd->optionalStoredAudio.empty()) {
			emitCode << "  std::vector<ddtype> stores = {";
			for (size_t idx2 = 0; idx2 < nd->optionalStoredAudio.size(); ++idx2) {
				auto d = nd->optionalStoredAudio[idx2];
				if (idx2 > 0) emitCode << ", ";
				emitCode << "{ .i = " << d.i << "LL }";
			}
			emitCode << "};\n";
		}

		// Input nodes
		if (nd->getType()->isInputNode) {
			const int pin = nd->inputIndex;
			emitCode << "  std::vector<ddtype> i0(inputs[" << pin << "], inputs[" << pin << "] + inputSizes[" << pin << "]);\n";
			emitCode << "  int isize0 = (int)i0.size();\n";
		}
		else {
			// Regular inputs
			for (int j = 0; j < nd->getNumInputs(); j++) {
				auto* inp = nd->getInput(j);
				if (inp) {
					const auto srcIdx = nodeFieldVars.at(inp);
					emitCode << "  std::vector<ddtype>& i" << j << " = field" << juce::String(srcIdx) << ";\n";
					emitCode << "  int isize" << j << " = size" << juce::String(srcIdx) << ";\n";

					// Conversion handling
					auto itype = nd->getType()->inputs[j].inputType;
					if (itype == InputType::any) itype = nd->getTrueType();
					auto otype = inp->getType()->outputType;
					if (otype == InputType::followsInput) otype = inp->getTrueType();

					if (otype != itype) {
						emitCode << "  std::vector<ddtype> i" << j << "_conv(isize" << j << ");\n";
						// emit conversion loop (similar to your old one, but writing into iX_conv)
						// ...
						emitCode << "  i" << j << " = i" << j << "_conv;\n";
					}
				}
				else {
					auto type = nd->getType()->inputs[j].inputType;
					if (type == InputType::any) type = nd->getTrueType();
					emitCode << "  std::vector<ddtype> i" << j << "(1);\n";
					if (type == InputType::decimal)
						emitCode << "  i" << j << "[0].d = " << juce::String(nd->defaultValues[j].d) << ";\n";
					else
						emitCode << "  i" << j << "[0].i = " << juce::String(nd->defaultValues[j].i) << ";\n";
					emitCode << "  int isize" << j << " = 1;\n";
				}
			}
		}

		emitCode << "  " << nd->getType()->emitCode(*nd, ord) << "\n";
		emitCode << "}\n\n";
		++ord;
	}

	return emitCode;
}


const char* ddtypeClang =
"#include <stdint.h>\n"
"typedef union {\n"
"    double d;\n"
"    int64_t i;\n"
"} ddtype;\n";

const juce::String ddtypeClangJ(ddtypeClang);

const juce::String clangHeader(uint64_t x) {
	return ddtypeClangJ + UserInputClangJ + "#include <cmath>\n#include <cstdlib>\nextern \"C\" void nodeTypeOutput" + juce::String(x) + "(ddtype * output, int outputSize, ddtype * *inputs, int* inputSizes, int numInputs) { ";
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

	for (auto& node : input.nodeCopies)
		findRemainingSizes(node.get(), input, outerInputs); // just warms the cache

	setupRecursive(input.outputNode, input);

	for (auto& [node, ownership] : input.safeOwnership) {
		if (node) {
			auto [offset, size] = ownership;
			input.nodeOwnership[node] = std::span<ddtype>(input.field.data() + offset, size);
		}
	}

	juce::String clangCode = initializeClang(input, scene, outerInputs);
	input.clangcode = (clangHeader(globalCompileCounter) + clangCode + clangCloser).toStdString();

	llvm::OptimizationLevel OX;
	switch (input.optLevel) {
	case OptLevel::high:           OX = llvm::OptimizationLevel::O3; break;
	case OptLevel::medium:         OX = llvm::OptimizationLevel::O2; break;
	case OptLevel::low:            OX = llvm::OptimizationLevel::O1; break;
	case OptLevel::minimal:        OX = llvm::OptimizationLevel::O0; break;
	case OptLevel::prioritizeSize: OX = llvm::OptimizationLevel::Oz; break;
	case OptLevel::tradeOffSize:   OX = llvm::OptimizationLevel::Os; break;
	}

	input.compiledFunc = compileNodeKernel(
		input.clangcode,
		(juce::String("nodeTypeOutput") + juce::String(globalCompileCounter)).toStdString(),
		OX);

	// Handle compile-time known nodes
	UserInput fakeInput;
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

			node->getType()->execute(*node, fakeInput, inputs, output, input);
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

