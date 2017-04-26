var doc = app.activeDocument;
if (doc != null) {
    var outFile = File.saveDialog("Save Layer Header file", "*.h", false);
    if (outFile instanceof File) {
        var filepath = outFile.toString();
        var fname = filepath.substring(filepath.lastIndexOf('/') + 1, filepath.lastIndexOf('.'));
        outFile.open("w");
        outFile.writeln("#pragma once");
        outFile.writeln("#include \"termite/vec_math.h\"");
        outFile.writeln("");
        outFile.writeln("struct PsLayer_" + fname + " { const char* name;  termite::vec2_t pos; termite::vec2_t size; };");
        outFile.writeln("static const PsLayer_" + fname + " kLayers_" + fname + "[] {");
        for (var i = 0; i < doc.layers.length; i++) {
            var layer = doc.layers[i];
            if (!layer.visible)
                continue;
            var str = "    {\"" + layer.name + ".png\", " + 
                      "termite::vec2f(" + parseInt(layer.bounds[0]) + ".0f, " + parseInt(layer.bounds[1]) + ".0f), " +
                      "termite::vec2f(" + parseInt(layer.bounds[2]-layer.bounds[0]) + ".0f, " + parseInt(layer.bounds[3]-layer.bounds[1]) + ".0f)}";
            if (i < doc.layers.length - 1)
              str += ", "
            outFile.writeln(str)
        }
        outFile.writeln("};");
        outFile.close();
    }
}

