// Exports function symbols already present in the loaded Ghidra program.
// This does not assign names or infer structure; names come from the loader/program.
import java.io.BufferedWriter;
import java.io.FileWriter;
import java.io.IOException;

import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.Function;

public class ExportFunctionMap extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] args = getScriptArgs();
        if (args.length != 1) {
            printerr("Usage: ExportFunctionMap.java <output.tsv>");
            return;
        }
        try (BufferedWriter out = new BufferedWriter(new FileWriter(args[0]))) {
            out.write("entry\tname\tthunk\texternal\n");
            for (Function function : currentProgram.getFunctionManager().getFunctions(true)) {
                out.write(function.getEntryPoint().toString());
                out.write('\t');
                out.write(function.getName().replace('\t', ' '));
                out.write('\t');
                out.write(Boolean.toString(function.isThunk()));
                out.write('\t');
                out.write(Boolean.toString(function.isExternal()));
                out.write('\n');
            }
        }
        println("Exported Ghidra function symbols to " + args[0]);
    }
}
