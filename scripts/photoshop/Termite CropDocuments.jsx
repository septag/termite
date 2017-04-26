if (app.documents.length > 0) {
    preferences.rulerUnits = Units.PIXELS;
    var layerRef = app.activeDocument.selection;
    var bounds = layerRef.bounds;
    
    for (var i = 0; i < app.documents.length; i++)  {
        var doc = app.documents[i];
        app.activeDocument = doc;
        doc.crop(bounds);
        doc.save();
    }    
}

