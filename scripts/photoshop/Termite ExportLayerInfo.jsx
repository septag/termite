function exportLayerData(outFile, layer, comma)
{
    if (!layer.visible)
        return;
    var str = "    {\"" + layer.name + ".png\", " + 
              "tee::vec2(" + parseInt(layer.bounds[0]) + ".0f, " + parseInt(layer.bounds[1]) + ".0f), " +
              "tee::vec2(" + parseInt(layer.bounds[2]-layer.bounds[0]) + ".0f, " + parseInt(layer.bounds[3]-layer.bounds[1]) + ".0f)}";
    if (comma)
      str += ", "
    outFile.writeln(str)
}

var doc = app.activeDocument;
if (doc != null) {
    var outFile = File.saveDialog("Save Layer Header file", "*.h", false);
    if (outFile instanceof File) {
        var filepath = outFile.toString();
        var fname = filepath.substring(filepath.lastIndexOf('/') + 1, filepath.lastIndexOf('.'));
        outFile.open("w");
        outFile.writeln("#pragma once");
        outFile.writeln("#include \"termite/tmath.h\"");
        outFile.writeln("");
        outFile.writeln("struct PsLayer_" + fname + " { const char* name;  tee::vec2_t pos; tee::vec2_t size; };");
        outFile.writeln("static const PsLayer_" + fname + " kLayers_" + fname + "[] {");

        var index = 0;
        // count all exported layers
        var count = doc.artLayers.length;
        for (var i = 0; i < doc.layerSets.length; i++)
            count += doc.layerSets[i].artLayers.length;

        // Export layer data
        for (var i = 0; i < doc.artLayers.length; i++) {
            exportLayerData(outFile, doc.artLayers[i], index < count - 1);
            index++;
        }

        // Do this for all top-level groups
        for (var i = 0; i < doc.layerSets.length; i++) {
            for (var k = 0; k < doc.layerSets[i].artLayers.length; k++) {
                exportLayerData(outFile, doc.layerSets[i].artLayers[k], index < count - 1);
                index++;
            }
        }

        outFile.writeln("};");
        outFile.close();
    }
}

