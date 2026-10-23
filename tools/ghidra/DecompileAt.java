// Export a single loader-defined function without running global auto-analysis.
// Arguments: address output-path. Intended for targeted evidence collection.
import java.io.FileWriter;
import java.io.PrintWriter;

import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.address.Address;
import ghidra.program.model.listing.Function;
import ghidra.util.task.ConsoleTaskMonitor;

public class DecompileAt extends ghidra.app.script.GhidraScript {
    @Override
    public void run() throws Exception {
        if (getScriptArgs().length < 2) {
            printerr("Usage: DecompileAt <address> <output-path>");
            return;
        }
        Address address = toAddr(getScriptArgs()[0]);
        String outputPath = getScriptArgs()[1];
        Function function = currentProgram.getFunctionManager().getFunctionContaining(address);
        if (function == null) {
            function = currentProgram.getFunctionManager().getFunctionAt(address);
        }
        if (function == null) {
            throw new IllegalStateException("No loader-defined function at " + address);
        }

        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        DecompileResults result = decompiler.decompileFunction(function, 90, new ConsoleTaskMonitor());
        try (PrintWriter out = new PrintWriter(new FileWriter(outputPath))) {
            out.println("// Program: " + currentProgram.getName());
            out.println("// Entry requested: " + address);
            out.println("// Function entry: " + function.getEntryPoint());
            out.println("// Function label: " + function.getName() + " (loader-generated unless separately verified)");
            out.println("// Signature: " + function.getSignature());
            out.println("// Body bytes: " + function.getBody().getNumAddresses());
            out.println("// Decompiler complete: " + result.decompileCompleted());
            if (result.getErrorMessage() != null && !result.getErrorMessage().isBlank()) {
                out.println("// Decompiler message: " + result.getErrorMessage());
            }
            if (result.getDecompiledFunction() != null) {
                out.println(result.getDecompiledFunction().getC());
            }
        }
        decompiler.dispose();
        println("Exported " + function.getEntryPoint() + " to " + outputPath +
            "; completed=" + result.decompileCompleted());
    }
}
