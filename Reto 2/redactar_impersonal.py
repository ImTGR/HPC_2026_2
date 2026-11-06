"""Cambia la voz del informe sin reconstruir estilos, tablas ni imagenes."""
from pathlib import Path
from zipfile import ZipFile
from lxml import etree
import re
import shutil
import json

ROOT=Path(__file__).resolve().parent
target=next((ROOT/'output'/'word').glob('*.docx'))
destination=ROOT/'output'/'word'/'impersonal'/target.name
destination.parent.mkdir(parents=True,exist_ok=True)
qa=ROOT/'tmp'/'word'/'impersonal'
qa.mkdir(parents=True,exist_ok=True)
backup=qa/'original.docx'
if not backup.exists():shutil.copy2(target,backup)

# Frases especificas: se conserva el significado y la concordancia gramatical.
REPLACEMENTS=[
 ('estudiamos dos maneras','se estudiaron dos maneras'),
 ('Primero revisamos que','Primero se verificó que'),
 ('Después repartimos los lanzamientos','Después se repartieron los lanzamientos'),
 ('y comparamos los tiempos','y se compararon los tiempos'),
 ('También comprobamos que','También se comprobó que'),
 ('No necesitamos conocer','No es necesario conocer'),
 ('Por eso podemos repartir','Por eso es posible repartir'),
 ('Realizamos las mediciones','Se realizaron las mediciones'),
 ('y usamos siempre los mismos datos','y se utilizaron siempre los mismos datos'),
 ('Podemos imaginar','Se puede imaginar'),
 ('usamos x² + y² ≤ 1','se utiliza la condición x² + y² ≤ 1'),
 ('En Needles imaginamos','En Needles se considera'),
 ('y lanzamos agujas sobre ella','y se lanzan agujas sobre ella'),
 ('generamos la distancia','se generan la distancia'),
 ('sabemos si alcanza','se determina si alcanza'),
 ('Llamamos L a la longitud de la aguja y D a la separación entre líneas.',
  'L representa la longitud de la aguja y D la separación entre líneas.'),
 ('obtenemos la aproximación','se obtiene la aproximación'),
 ('En nuestro programa','En el programa'),
 ('no podemos dividir','no es posible dividir'),
 ('Seguimos esa formulación','Se mantuvo esa formulación'),
 ('si cambiamos las muestras','al cambiar las muestras'),
 ('Qué comprobamos antes de paralelizar','Qué se comprobó antes de paralelizar'),
 ('Revisamos Needle.c y Dartboard.c y los compilamos',
  'Se revisaron los archivos Needle.c y Dartboard.c y se compilaron'),
 ('Los compilamos sin activar OpenMP','Se compilaron sin activar OpenMP'),
 ('Conservamos estos archivos','Se conservaron estos archivos'),
 ('No esperábamos','No se esperaba'),
 ('Cómo repartimos el trabajo con OpenMP','Cómo se repartió el trabajo con OpenMP'),
 ('Empleamos una mezcla','Se empleó una mezcla'),
 ('Esto nos permite comprobar','Esto permite comprobar'),
 ('Usamos parallel for','Se usó parallel for'),
 ('decidimos medir','se decidió medir'),
 ('Solicitamos 1, 2, 4, 6 o 12 hilos y registramos cuántos participaron realmente.',
  'Se solicitaron 1, 2, 4, 6 o 12 hilos y se registró cuántos participaron realmente.'),
 ('Desactivamos el cambio','Se desactivó el cambio'),
 ('Qué optimizamos en el procesador y en la memoria','Qué se optimizó en el procesador y en la memoria'),
 ('lo que buscamos obtener','el resultado esperado'),
 ('Probamos tres alternativas','Se probaron tres alternativas'),
 ('También simplificamos','También se simplificó'),
 ('Conservamos la prueba','Se conservó la prueba'),
 ('y usamos funciones pequeñas','y se utilizaron funciones pequeñas'),
 ('No medimos por separado cada simplificación','No se midió por separado cada simplificación'),
 ('no le asignamos un porcentaje','no se le asignó un porcentaje'),
 ('Probamos SIMD','Se probó SIMD'),
 ('Añadimos parallel for simd y comparamos sus tiempos','Se añadió parallel for simd y se compararon sus tiempos'),
 ('También revisamos los mensajes','También se revisaron los mensajes'),
 ('Por eso la presentamos','Por eso se presenta'),
 ('Para medir el efecto creamos buffered','Para medir el efecto se creó buffered'),
 ('Mantuvimos esa buena decisión y agregamos la versión',
  'Se mantuvo esa buena decisión y se agregó la versión'),
 ('Cómo medimos el desempeño','Cómo se midió el desempeño'),
 ('No fijamos un núcleo específico para cada hilo ni controlamos la temperatura',
  'No se fijó un núcleo específico para cada hilo ni se controlaron la temperatura'),
 ('Usamos 100 mil, un millón y diez millones de lanzamientos.',
  'Se utilizaron 100 mil, un millón y diez millones de lanzamientos.'),
 ('probamos once configuraciones','se probaron once configuraciones'),
 ('Antes de medir hicimos calentamientos.','Antes de medir se realizaron calentamientos.'),
 ('Así medimos la variación','Así se midió la variación'),
 ('Qué tiempo y qué memoria registramos','Qué tiempo y qué memoria se registraron'),
 ('Medimos por separado el cálculo y la ejecución','Se midieron por separado el cálculo y la ejecución'),
 ('En Windows usamos QueryPerformanceCounter','En Windows se utilizó QueryPerformanceCounter'),
 ('Descartamos una campaña inicial','Se descartó una campaña inicial'),
 ('y repetimos las mediciones','y se repitieron las mediciones'),
 ('También registramos el tiempo CPU','También se registró el tiempo CPU'),
 ('publicamos la mediana','se publicó la mediana'),
 ('Conservamos también el menor, el mayor y una medida','Se conservaron también el menor, el mayor y una medida'),
 ('Medimos también cuántos bytes','También se registró cuántos bytes'),
 ('No medimos los fallos de caché ni la velocidad','No se midieron los fallos de caché ni la velocidad'),
 ('Para la referencia usamos','Para la referencia se utilizaron'),
 ('Comparamos los modos','Se compararon los modos'),
 ('En las pruebas de reparto usamos','En las pruebas de reparto se utilizaron'),
 ('Para las pruebas con varias semillas usamos un millón de muestras y un margen',
  'Para las pruebas con varias semillas se trabajó con un millón de muestras y un margen'),
 ('Por qué también medimos la ejecución completa','Por qué también se midió la ejecución completa'),
 ('El benchmark nos dice','El benchmark indica'),
 ('Primero intentamos usar gprof','Primero se intentó usar gprof'),
 ('Compilamos la versión preparada para esa herramienta y la ejecutamos',
  'Se compiló la versión preparada para esa herramienta y se ejecutó'),
 ('medimos por separado dos partes','se midieron por separado dos partes'),
 ('Usamos treinta millones de lanzamientos, descartamos un calentamiento y tomamos cinco ejecuciones por algoritmo.',
  'Se utilizaron treinta millones de lanzamientos, se descartó un calentamiento y se registraron cinco ejecuciones por algoritmo.'),
 ('las partes que queremos estudiar','las partes de interés'),
 ('que hayamos medido','que se haya medido'),
 ('no presentamos SIMD','no se presenta SIMD'),
 ('Cuando repartimos muchos lanzamientos','Al repartir muchos lanzamientos'),
 ('no medimos por separado todas las causas','no se midieron por separado todas las causas'),
 ('Qué podemos afirmar con estas pruebas','Qué se puede afirmar con estas pruebas'),
 ('Tampoco medimos por separado el costo','Tampoco se midieron por separado el costo'),
 ('En Dartboard contamos puntos dentro del círculo; en Needles contamos agujas que cruzan.',
  'En Dartboard se cuentan puntos dentro del círculo; en Needles se cuentan agujas que cruzan.'),
 ('no elegimos una sola repetición favorable','no se eligió una sola repetición favorable'),
 ('Qué buscamos','Propósito'),
 ('Qué revisamos','Qué se revisó'),
]

NS={'w':'http://schemas.openxmlformats.org/wordprocessingml/2006/main'}
XML_SPACE='{http://www.w3.org/XML/1998/namespace}space'

def text_of(root):return ''.join(root.xpath('.//w:t/text()',namespaces=NS))

def replace_phrase(par,old,new):
    count=0
    while True:
        nodes=par.xpath('.//w:t',namespaces=NS)
        text=''.join(n.text or '' for n in nodes)
        start=text.find(old)
        if start<0:return count
        stop=start+len(old)
        cursor=0;inserted=False
        for node in nodes:
            value=node.text or '';end=cursor+len(value)
            if cursor<stop and end>start:
                left=max(0,start-cursor);right=min(len(value),stop-cursor)
                node.text=value[:left]+(new if not inserted else '')+value[right:]
                node.set(XML_SPACE,'preserve')
                inserted=True
            cursor=end
        count+=1

with ZipFile(target) as z:
    parts={i.filename:z.read(i.filename) for i in z.infolist()}
    infos=z.infolist()
root=etree.fromstring(parts['word/document.xml'])
before=text_of(root)
log=[]
for par in root.xpath('//w:p',namespaces=NS):
    for old,new in REPLACEMENTS:
        count=replace_phrase(par,old,new)
        if count:log.append({'before':old,'after':new,'occurrences':count})
after=text_of(root)
# Los resultados y las cantidades se conservan exactamente.
assert re.findall(r'\d+(?:[.,]\d+)*',before)==re.findall(r'\d+(?:[.,]\d+)*',after)
false_positives={'algoritmos','mismos','mínimos','máximos','próximos','últimos'}
scan='\n'.join(text_of(par) for par in root.xpath('//w:p',namespaces=NS))
left=sorted(set(w for w in re.findall(r'\b\w+mos\b',scan,re.I) if w.lower() not in false_positives))
assert not left,left
assert not re.search(r'\b(nuestro\w*|nosotros|nos|hayamos|podemos|queremos)\b',scan,re.I)
# Mantiene el pie de la tabla de escalabilidad junto a su ultima fila.
tables=root.xpath('//w:tbl',namespaces=NS)
for par in tables[7].xpath('./w:tr[last()]/w:tc/w:p',namespaces=NS):
    tag='{'+NS['w']+'}'
    ppr=par.find(tag+'pPr')
    if ppr is None:
        ppr=etree.Element(tag+'pPr');par.insert(0,ppr)
    keep=ppr.find(tag+'keepNext')
    if keep is None:
        keep=etree.Element(tag+'keepNext');ppr.insert(0,keep)
    keep.set(tag+'val','true')
updated=etree.tostring(root,xml_declaration=True,encoding='UTF-8',standalone=True)
temp=qa/'impersonal.docx'
with ZipFile(temp,'w') as z:
    for info in infos:z.writestr(info,updated if info.filename=='word/document.xml' else parts[info.filename])
with ZipFile(temp) as z:
    assert all(z.read(n)==v for n,v in parts.items() if n!='word/document.xml')
shutil.copy2(temp,destination)
(qa/'cambios.json').write_text(json.dumps(log,ensure_ascii=False,indent=2),encoding='utf-8')
print('Documento actualizado:',destination)
print('Sustituciones:',sum(x['occurrences'] for x in log))
print('Numeros conservados y sin expresiones de primera persona detectadas.')
