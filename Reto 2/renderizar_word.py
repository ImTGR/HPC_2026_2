"""Usa el renderizador de documentos con el PDF exportado por Word en Windows.
Word sustituye solamente la conversion DOCX-PDF; el renderer empaquetado
conserva su calculo de resolucion y su generacion de PNGs para la revision.
"""
from pathlib import Path
import importlib.util
import sys

root=Path(__file__).resolve().parent
script=Path(r'C:\Users\Samuel\.codex\plugins\cache\openai-primary-runtime\documents\26.1030.10813\skills\documents\render_docx.py')
spec=importlib.util.spec_from_file_location('codex_render_docx',script)
renderer=importlib.util.module_from_spec(spec)
spec.loader.exec_module(renderer)
pdf=root/'tmp'/'word'/'revision'/'word.pdf'
assert pdf.is_file() and pdf.stat().st_size>0
def native_conversion(doc_path,user_profile,convert_tmp_dir,stem,verbose):
    return str(pdf),'Conversion nativa realizada con Microsoft Word; indice y campos actualizados.'
renderer.convert_to_pdf=native_conversion
renderer.main()
