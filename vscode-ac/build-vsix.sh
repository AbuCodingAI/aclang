#!/usr/bin/env bash
# Build vscode-ac-<version>.vsix manually (Node 18 workaround — vsce needs Node 20+)
set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
VERSION=$(python3 -c "import json; print(json.load(open('$DIR/package.json'))['version'])")
PUBLISHER=$(python3 -c "import json; print(json.load(open('$DIR/package.json'))['publisher'])")
OUT="$DIR/vscode-ac-$VERSION.vsix"

TMP="$(mktemp -d)"
EXT="$TMP/extension"

# Removed "$EXT/themes" since we are no longer compiling a global icon theme
mkdir -p "$EXT/syntaxes" "$EXT/icons"

cp "$DIR/package.json" "$EXT/"
cp "$DIR/language-configuration.json" "$EXT/"
cp "$DIR/syntaxes/ac.tmLanguage.json" "$EXT/syntaxes/"
cp "$DIR/icons/ac-file.png" "$EXT/icons/"
cp "$DIR/icons/ac-logo.png" "$EXT/icons/"

cat > "$TMP/[Content_Types].xml" << 'EOF'
<?xml version="1.0" encoding="utf-8"?>
<Types xmlns="http://openxmlformats.org">
  <Default Extension="json" ContentType="application/json"/>
  <Default Extension="png" ContentType="image/png"/>
  <Default Extension="vsixmanifest" ContentType="text/xml"/>
</Types>
EOF

cat > "$TMP/extension.vsixmanifest" << EOF
<?xml version="1.0" encoding="utf-8"?>
<PackageManifest Version="2.0.0" xmlns="http://microsoft.com" xmlns:d="http://microsoft.com">
  <Metadata>
    
    <DisplayName>AC Language</DisplayName>
    <Description xml:space="preserve">Syntax highlighting and file icons for the AC programming language</Description>
    <Tags>ac,programming,syntax</Tags>
    <Categories>Programming Languages</Categories>
    <GalleryFlags>Public</GalleryFlags>
    <Properties>
      <Property Id="Microsoft.VisualStudio.Code.Engine" Value="^1.75.0"/>
    </Properties>
  </Metadata>
  <Installation>
    <InstallationTarget Id="Microsoft.VisualStudio.Code"/>
  </Installation>
  <Assets>
    <Asset Type="Microsoft.VisualStudio.Code.Manifest" Path="extension/package.json" Addressable="true"/>
    <Asset Type="Microsoft.VisualStudio.Services.Icons.Default" Path="extension/icons/ac-logo.png" Addressable="true"/>
    <Asset Type="Microsoft.VisualStudio.Code.LanguageConfiguration" Path="extension/language-configuration.json" Addressable="true"/>
    <Asset Type="Microsoft.VisualStudio.Code.Grammar" Path="extension/syntaxes/ac.tmLanguage.json" Addressable="true"/>
  </Assets>
</PackageManifest>
EOF

cd "$TMP" && zip -r "$OUT" . -x "*.DS_Store"
rm -rf "$TMP"
echo "Built: $OUT"
