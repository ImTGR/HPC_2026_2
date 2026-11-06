"""Version editable con explicaciones sencillas y los resultados conservados."""
from pathlib import Path
from html import unescape
import re
from docx import Document
from docx.shared import Inches, Pt, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.enum.table import WD_TABLE_ALIGNMENT, WD_CELL_VERTICAL_ALIGNMENT
from docx.oxml import OxmlElement
from docx.oxml.ns import qn

# El modulo solo prepara los datos y el contenido del PDF; no lo sobrescribe.
import crear_informe_pdf as data

ROOT=Path(__file__).resolve().parent
OUT=ROOT/'output'/'word'
OUT.mkdir(parents=True,exist_ok=True)
NAME=data.NAME.replace('.pdf','.docx')
doc=Document()
sec=doc.sections[0]
sec.page_width=Inches(8.5); sec.page_height=Inches(11)
sec.left_margin=Pt(70.55); sec.right_margin=Pt(70.55)
sec.top_margin=Pt(62); sec.bottom_margin=Pt(54)
sec.footer_distance=Pt(24)
CW=470.9

def font(style,size=12,bold=False,italic=False):
    s=doc.styles[style]
    s.font.name='Times New Roman';s.font.size=Pt(size)
    s.font.bold=bold;s.font.italic=italic;s.font.color.rgb=RGBColor(0,0,0)
    rpr=s._element.get_or_add_rPr()
    rf=rpr.get_or_add_rFonts()
    for key in list(rf.attrib):
        if 'theme' in key.lower():del rf.attrib[key]
    for key in ('ascii','hAnsi','eastAsia','cs'):rf.set(qn('w:'+key),'Times New Roman')
    for child in list(rpr):
        if child.tag in (qn('w:spacing'),qn('w:kern')):rpr.remove(child)
    if hasattr(s,'paragraph_format'):
        s.paragraph_format.line_spacing=Pt(15)
        s.paragraph_format.space_after=Pt(7)
    return s

normal=font('Normal'); normal.paragraph_format.alignment=WD_ALIGN_PARAGRAPH.JUSTIFY
normal.paragraph_format.widow_control=True
for style in ('Title','Subtitle','Heading 1','Heading 2','Heading 3'):
    st=font(style,bold=style!='Subtitle')
    st.paragraph_format.keep_with_next=True
    st.paragraph_format.space_before=Pt(9 if style.startswith('Heading') else 0)
    st.paragraph_format.space_after=Pt(6)
    # Evita reglas decorativas o colores de los estilos predeterminados.
    pr=st._element.get_or_add_pPr()
    for border in list(pr.findall(qn('w:pBdr'))): pr.remove(border)
font('Caption',10.5,italic=True).paragraph_format.alignment=WD_ALIGN_PARAGRAPH.CENTER
for name in ('TOC Heading','TOC 1','Hyperlink','FollowedHyperlink'):
    if name not in doc.styles:
        from docx.enum.style import WD_STYLE_TYPE
        doc.styles.add_style(name,WD_STYLE_TYPE.PARAGRAPH if name.startswith('TOC') else WD_STYLE_TYPE.CHARACTER)
    font(name,12,bold=name=='TOC Heading')
    if name=='TOC 1':
        doc.styles[name].paragraph_format.line_spacing=Pt(19)
        doc.styles[name].paragraph_format.space_after=Pt(1)

foot=sec.footer.paragraphs[0]
foot.alignment=WD_ALIGN_PARAGRAPH.CENTER
f=OxmlElement('w:fldSimple');f.set(qn('w:instr'),'PAGE')
foot._p.append(f)
foot.paragraph_format.space_after=Pt(0)
update=OxmlElement('w:updateFields');update.set(qn('w:val'),'true')
doc.settings.element.append(update)
doc.core_properties.title='Cálculo de pi con Monte Carlo y OpenMP'
doc.core_properties.author='Samuel Herrera Marin; Jonathan Gaviria Ocampo; Tomás Marulanda Aristizabal'
doc.core_properties.subject='Reto 2 de High Performance Computing'

def p(text='',style=None,center=False,keep=False):
    par=doc.add_paragraph(text,style)
    if center:par.alignment=WD_ALIGN_PARAGRAPH.CENTER
    if keep:par.paragraph_format.keep_with_next=True
    return par
def h(text):return p(text,'Heading 1')
def sub(text):return p(text,'Heading 2')
def formula(text):return p(text,center=True)
def code(text):
    lines=text.strip().splitlines()
    for i,line in enumerate(lines):
        q=p(line)
        q.alignment=WD_ALIGN_PARAGRAPH.LEFT
        q.paragraph_format.space_before=Pt(0)
        q.paragraph_format.space_after=Pt(7 if i==len(lines)-1 else 0)
        q.paragraph_format.line_spacing=Pt(11)
        q.paragraph_format.keep_with_next=i<len(lines)-1
        for run in q.runs:run.font.name='Consolas';run.font.size=Pt(9)

TABLE_NO=0
def table(headers,rows,widths=None,total=False,caption=''):
    global TABLE_NO
    TABLE_NO+=1
    widths=widths or [CW/len(headers)]*len(headers)
    assert abs(sum(widths)-CW)<.2
    t=doc.add_table(rows=1,cols=len(headers))
    t.alignment=WD_TABLE_ALIGNMENT.CENTER;t.autofit=False
    for col,width in zip(t.columns,widths):col.width=Pt(width)
    for cell,width in zip(t.rows[0].cells,widths):cell.width=Pt(width)
    for r in rows:t.add_row()
    values=[headers]+rows
    for ri,(row,vals) in enumerate(zip(t.rows,values)):
        trpr=row._tr.get_or_add_trPr()
        trpr.append(OxmlElement('w:cantSplit'))
        if ri==0:trpr.append(OxmlElement('w:tblHeader'))
        for ci,(cell,text,width) in enumerate(zip(row.cells,vals,widths)):
            cell.width=Pt(width);cell.vertical_alignment=WD_CELL_VERTICAL_ALIGNMENT.CENTER
            tcpr=cell._tc.get_or_add_tcPr()
            margins=OxmlElement('w:tcMar')
            for side in ('top','bottom','left','right'):
                el=OxmlElement('w:'+side);el.set(qn('w:w'),'70' if side in ('top','bottom') else '85');el.set(qn('w:type'),'dxa');margins.append(el)
            tcpr.append(margins)
            borders=OxmlElement('w:tcBorders')
            for edge in ('top','left','bottom','right'):
                el=OxmlElement('w:'+edge);el.set(qn('w:val'),'single');el.set(qn('w:sz'),'4');el.set(qn('w:color'),'000000');borders.append(el)
            tcpr.append(borders)
            if ri==0 or (total and ri==len(values)-1):
                shade=OxmlElement('w:shd');shade.set(qn('w:fill'),'4F81BD' if ri==0 else 'B7DEE8');tcpr.append(shade)
            q=cell.paragraphs[0];q.text=str(text)
            q.alignment=WD_ALIGN_PARAGRAPH.CENTER if ri==0 or ci>0 else WD_ALIGN_PARAGRAPH.LEFT
            q.paragraph_format.line_spacing=Pt(12.5)
            q.paragraph_format.space_before=Pt(0);q.paragraph_format.space_after=Pt(0)
            q.paragraph_format.keep_with_next=False
            for run in q.runs:run.font.name='Times New Roman';run.font.size=Pt(10.5);run.bold=ri==0
    if caption:p(f'Tabla {TABLE_NO}. {caption}','Caption')
    else:p().paragraph_format.space_after=Pt(2)
    return t

def figure(name,caption):
    q=p();q.alignment=WD_ALIGN_PARAGRAPH.CENTER;q.paragraph_format.keep_with_next=True
    # La altura de linea debe crecer con la imagen; una altura exacta la recorta.
    q.paragraph_format.line_spacing=1.0
    picture=q.add_run().add_picture(str(ROOT/'resultados'/name),width=Pt(CW))
    picture._inline.docPr.set('descr',caption)
    p(caption,'Caption')

# Portada con la misma tipografia y distribucion general del PDF.
t=p('Cálculo de pi con los algoritmos Needles y Dartboard','Title',center=True)
p('Versiones seriales y paralelas con OpenMP','Subtitle',center=True)
t=p('Reto 2',center=True);t.runs[0].bold=True
t.paragraph_format.space_after=Pt(110)
p('Presentado por:',center=True)
for name in ('Samuel Herrera Marin','Jonathan Gaviria Ocampo','Tomás Marulanda Aristizabal'):
    q=p(name,center=True);q.paragraph_format.space_after=Pt(0)
doc.paragraphs[-1].paragraph_format.space_after=Pt(139)
p('Presentado a:',center=True)
q=p('Ramiro Andrés Barrios Valencia',center=True);q.paragraph_format.space_after=Pt(158)
for text in ('HPC: High Performance Computing','Ingeniería de Sistemas y Computación','2026'):
    q=p(text,center=True);q.paragraph_format.space_after=Pt(0)
doc.add_page_break()

p('Tabla de contenido','TOC Heading')
toc=p();toc.alignment=WD_ALIGN_PARAGRAPH.LEFT
begin=OxmlElement('w:fldChar');begin.set(qn('w:fldCharType'),'begin')
instruction=OxmlElement('w:instrText');instruction.set(qn('xml:space'),'preserve');instruction.text=' TOC \\o "1-1" \\h \\z '
separate=OxmlElement('w:fldChar');separate.set(qn('w:fldCharType'),'separate')
end=OxmlElement('w:fldChar');end.set(qn('w:fldCharType'),'end')
for el in (begin,instruction,separate,end):toc.add_run()._r.append(el)
doc.add_page_break()

h('Resumen')
p('En este reto estudiamos dos maneras de calcular una aproximación de π usando muchos lanzamientos '
  'aleatorios: Dartboard y Needles. Primero revisamos que los programas originales funcionaran con un '
  'solo hilo. Después repartimos los lanzamientos entre varios hilos con OpenMP y comparamos los '
  'tiempos, el uso de memoria y las partes del programa que demandaban más trabajo.')
p('La revisión incluyó 163 comprobaciones de funcionamiento y 990 mediciones de tiempo. Con diez '
  'millones de lanzamientos, el menor tiempo central de cinco ejecuciones fue de 5,646 milisegundos '
  'en Dartboard y 41,266 milisegundos en Needles. En ambos casos se usaron doce hilos. También '
  'comprobamos que guardar todos los datos aumentaba la memoria del proceso de unos 4,1 a 156,7 MiB. '
  'Estos resultados muestran que repartir el trabajo ayuda, pero que la cantidad de muestras, '
  'la forma de repartirlas y el almacenamiento también influyen.')

h('1 Introducción')
p('Los algoritmos de Monte Carlo utilizan experimentos aleatorios para aproximar una cantidad. '
  'En este caso, cada experimento consiste en lanzar un punto o una aguja. No necesitamos conocer '
  'el resultado del lanzamiento anterior para evaluar el siguiente. Por eso podemos repartir '
  'los lanzamientos entre varios hilos, es decir, varios grupos de trabajo dentro del programa.')
p('OpenMP permite expresar ese reparto en C sin crear y administrar cada hilo manualmente. '
  'El propósito del reto fue comprobar si ambos algoritmos podían aprovecharlo y entender por '
  'qué algunas decisiones mejoran el tiempo mientras otras aportan poco o incluso lo aumentan. '
  'Realizamos las mediciones en un solo computador y usamos siempre los mismos datos para '
  'comparar de forma justa las versiones nuevas.')

h('2 Objetivos')
sub('2.1 Objetivo general')
p('Implementar y comparar versiones seriales y paralelas de Needles y Dartboard para estimar π, '
  'y explicar sus tiempos y su uso de memoria a partir de pruebas y mediciones.')
sub('2.2 Objetivos específicos')
for text in [
 'Explicar cómo calcula π cada algoritmo y qué condiciones debe cumplir.',
 'Comprobar que los programas originales se ejecutan con un solo hilo y producen resultados coherentes.',
 'Repartir los lanzamientos con OpenMP sin perder aciertos ni cambiar las muestras.',
 'Comparar diferentes cantidades de hilos, formas de reparto y opciones de compilación.',
 'Medir el tiempo total del cálculo, el tiempo de sus partes y la memoria utilizada.',
 'Relacionar las diferencias observadas con las decisiones tomadas en el programa.']:
    p('• '+text)

h('3 Cómo funcionan los algoritmos')
sub('3.1 Dartboard o lanzamiento de puntos')
p('Podemos imaginar un círculo de radio uno dentro de un cuadrado de lado dos. El programa '
  'lanza puntos al azar sobre el cuadrado y cuenta cuántos quedan dentro del círculo. El área '
  'del círculo es π y la del cuadrado es cuatro. Por eso, la proporción de puntos que cae '
  'dentro del círculo se acerca a π/4 cuando se realizan muchos lanzamientos.')
formula('π estimado = 4 × puntos dentro del círculo / total de puntos')
p('Para saber si un punto está dentro, usamos x² + y² ≤ 1. Esta comparación evita calcular '
  'una raíz cuadrada y ya estaba presente en la versión original. El algoritmo solo necesita '
  'conservar las coordenadas del punto actual y el conteo acumulado.')
sub('3.2 Needles o aguja de Buffon')
p('En Needles imaginamos una superficie con líneas paralelas y lanzamos agujas sobre ella. '
  'En cada lanzamiento generamos la distancia del centro de la aguja a la línea más cercana '
  'y el ángulo de la aguja. Con esos dos datos sabemos si alcanza a cruzar una línea.')
p('Llamamos L a la longitud de la aguja y D a la separación entre líneas. La fórmula usada '
  'requiere que la aguja no sea más larga que esa separación. Si d es la distancia y θ el '
  'ángulo, la condición de cruce es d ≤ (L/2) × sin(θ). A partir de la proporción de cruces '
  'obtenemos la aproximación:')
formula('π estimado = 2 × L × total de agujas / (D × agujas que cruzan)')
p('En nuestro programa L=D=1, por lo que la expresión queda en 2 × total de agujas / cruces. '
  'Si ninguna aguja cruza, no podemos dividir entre cero y el programa indica que no hay '
  'estimación. Needles realiza un cálculo de seno en cada lanzamiento, lo cual lo hace '
  'más costoso que Dartboard.')
p('Esta versión de Needles utiliza una constante π conocida para generar el ángulo. '
  'Seguimos esa formulación del programa original como ejercicio de simulación; no se '
  'presenta como una manera independiente de descubrir nuevos dígitos de π.')
sub('3.3 Qué significa obtener una aproximación')
p('Ninguno de los dos métodos garantiza los mismos decimales en cada ejecución. El azar '
  'hace que los resultados cambien si cambiamos las muestras. En general, usar cien veces '
  'más lanzamientos reduce unas diez veces el error típico, aunque una ejecución concreta '
  'pueda quedar más cerca o más lejos del valor real.')
p('Con la misma cantidad de muestras, Dartboard tiene menor variación esperada que Needles '
  'para L=D. Según la comparación matemática, Needles necesita aproximadamente 2,09 veces '
  'las muestras para alcanzar una precisión típica parecida. Usar más hilos con la misma '
  'cantidad de lanzamientos mejora el tiempo; no mejora por sí solo la precisión.')

h('4 Revisión de las versiones seriales')
sub('4.1 Qué comprobamos antes de paralelizar')
p('Revisamos Needle.c y Dartboard.c y los compilamos nuevamente desde sus fuentes. Ambos '
  'realizan el cálculo en un único ciclo y no incluyen instrucciones OpenMP ni creación '
  'de hilos. Los compilamos sin activar OpenMP y no aparecieron advertencias. Conservamos '
  'estos archivos sin cambios como punto de partida del trabajo.')
p('Los originales hacen un millón de lanzamientos y toman la hora como punto de partida '
  'para generar los números aleatorios. Por eso sus respuestas pueden cambiar al ejecutarlos '
  'otra vez. Las fórmulas revisadas corresponden a las condiciones explicadas en la sección anterior.')
rows=[]
for key,label,hitkey in [('Dartboard','Dartboard','Puntos dentro'),('Needle','Needles','Agujas que cruzan')]:
    text=data.VERIFY['original_programs'][key]
    rows.append([label,re.search(hitkey+r': (\d+)',text).group(1),
                 re.search(r'PI aproximado: ([0-9.]+)',text).group(1).replace('.',','),
                 data.num(re.search(r'Tiempo: ([0-9.]+)',text).group(1))])
table(['Algoritmo','Aciertos o cruces','π estimado','Tiempo impreso (s)'],rows,
      [75,112,148,CW-335],caption='Resultados de los originales con un millón de lanzamientos. Fuente: verificacion.json.')
p('Ambas aproximaciones quedaron dentro del margen de variación aceptado por la prueba. '
  'No esperábamos una cantidad fija de decimales correctos. Comprobar una ejecución tampoco '
  'demuestra por sí solo que un generador de números aleatorios sea adecuado para cualquier estudio.')
sub('4.2 Una base serial para comparar tiempos')
p('Además de los originales, montecarlo.c tiene un modo serial nuevo. Hace el mismo '
  'trabajo que la versión paralela, con el mismo generador y la misma semilla, pero sin '
  'abrir una región de trabajo entre varios hilos. La semilla es el valor inicial que '
  'permite repetir las mismas muestras. Esta base es necesaria porque comparar directamente '
  'el original con la versión nueva mezclaría el efecto del paralelismo con el cambio de generador.')

h('5 Cómo repartimos el trabajo con OpenMP')
sub('5.1 Las mismas muestras con cualquier cantidad de hilos')
p('La función uniform01 calcula dos valores para cada lanzamiento a partir de su número '
  'y de una semilla común. El lanzamiento cero usa las posiciones cero y uno; el siguiente '
  'usa dos y tres, y así sucesivamente. Empleamos una mezcla de enteros basada en SplitMix64 [1].')
p('La ventaja práctica es que cada hilo puede generar sus muestras sin esperar al anterior '
  'ni modificar el estado de otro hilo. Si un lanzamiento cambia de hilo, conserva sus '
  'mismos valores. Esto nos permite comprobar que una diferencia de tiempo no se debe a '
  'haber cambiado las muestras. Las pruebas del reto no sustituyen una evaluación '
  'especializada de la calidad del generador.')
sub('5.2 Cada hilo cuenta sus propios aciertos')
p('Usamos parallel for para repartir las vueltas del ciclo. Cada hilo evalúa los '
  'lanzamientos que recibe y lleva un conteo propio. Al terminar, reduction(+:hits) suma '
  'esos conteos [2]. Por ejemplo, si tres hilos obtienen 3, 2 y 4 aciertos, el total final es 9.')
code('''#pragma omp parallel for default(none) shared(n, seed) \\
        schedule(runtime) reduction(+:hits)''')
p('La reducción evita que todos intenten cambiar el mismo contador en cada lanzamiento. '
  'Esa actualización simultánea podría perder aciertos si no se protege. Otra solución '
  'sería obligar a los hilos a turnarse cada vez que suman, pero eso añadiría muchas esperas. '
  'Con la reducción solo hay que combinar los resultados al final. Las variables del punto '
  'o de la aguja se usan de forma local, mientras N y la semilla solo se consultan.')
sub('5.3 Formas de entregar los lanzamientos a los hilos')
table(['Opción','Cómo reparte el trabajo'],[
 ['static','Entrega bloques definidos de iteraciones a los hilos.'],
 ['dynamic','Cada hilo solicita otro bloque de 1024 muestras cuando termina el suyo.'],
 ['guided','Empieza con bloques grandes y los reduce; el mínimo usado fue 1024.']],
 [82,CW-82],caption='Formas de reparto evaluadas con schedule(runtime) [3].')
p('static suele ser una buena primera opción si todos avanzan a un ritmo parecido. '
  'dynamic y guided permiten que un hilo que termina antes tome más trabajo. Esa '
  'flexibilidad tiene un costo de organización, por lo que decidimos medir las tres '
  'alternativas en lugar de suponer cuál ganaría. En esta campaña dynamic fue mejor '
  'con doce hilos, aunque eso puede cambiar en otro equipo.')
p('Solicitamos 1, 2, 4, 6 o 12 hilos y registramos cuántos participaron realmente. '
  'Desactivamos el cambio automático del tamaño del equipo. Esto es distinto del '
  'reparto dynamic, que decide qué bloque recibe cada hilo, no cuántos hilos hay.')

h('6 Qué optimizamos en el procesador y en la memoria')
sub('6.1 Opciones del compilador')
p('El compilador transforma el código C en un programa ejecutable. Sus opciones pueden '
  'cambiar la forma de hacer los cálculos sin cambiar lo que buscamos obtener. Probamos '
  'tres alternativas para ver si una mejoraba los tiempos [5].')
table(['Nombre','Opciones','Qué buscamos'],[
 ['O2','-O2','Una versión optimizada de uso general.'],
 ['O3','-O3','Permitir más mejoras automáticas del compilador.'],
 ['native','-O3 -march=native','Aprovechar las instrucciones disponibles en este procesador.']],
 [62,153,CW-215],caption='Compilaciones usadas en la comparación de tiempos.')
p('También simplificamos la condición de Needles para L=D=1, cancelando los factores '
  '1/2 de ambos lados. Conservamos la prueba de Dartboard sin raíz cuadrada y usamos '
  'funciones pequeñas para facilitar el trabajo del compilador. No medimos por separado '
  'cada simplificación, por lo que no le asignamos un porcentaje individual de mejora.')
sub('6.2 Prueba de varias operaciones por instrucción')
p('Probamos SIMD, una técnica que permite que una instrucción del procesador trabaje '
  'con varios datos a la vez. Añadimos parallel for simd y comparamos sus tiempos con '
  'los del reparto normal [4]. También revisamos los mensajes del compilador para '
  'comprobar qué partes logró transformar.')
p('El resultado fue que SIMD no dio una mejora constante. Escribir la instrucción '
  'en el código no asegura que todo el cálculo pueda aprovecharla. Por eso la '
  'presentamos como una técnica evaluada y no como una mejora demostrada para ambos algoritmos.')
sub('6.3 Generar y usar cada muestra de inmediato')
p('La versión normal genera un punto o una aguja, lo evalúa y continúa con el '
  'siguiente. Solo conserva los datos actuales y el conteo. Así no necesita guardar '
  'millones de muestras ni reservar memoria dentro del ciclo.')
p('Para medir el efecto creamos buffered, una versión de comparación que primero '
  'guarda todos los valores en dos arreglos y después los evalúa. Cada arreglo contiene '
  'N números de ocho bytes, de modo que ambos requieren 16N bytes. Con diez millones '
  'de muestras son unos 152,59 MiB adicionales.')
p('Los programas originales ya evitaban guardar todas las muestras. Mantuvimos '
  'esa buena decisión y agregamos la versión con arreglos para comprobar su costo. '
  'En la versión paralela, cada hilo necesita sus datos locales y su conteo, pero '
  'la memoria no crece con cada nuevo lanzamiento.')

h('7 Cómo medimos el desempeño')
sub('7.1 Equipo utilizado')
table(['Componente','Valor registrado'],[
 ['Procesador',data.ENV['cpu']],['Procesadores lógicos','12'],
 ['Sistema operativo','Windows 10, versión 19045, 64 bits'],
 ['Compilador','GCC 16.1.0 de MSYS2 / MinGW-w64'],['Python',data.ENV['python']],
 ['Fecha de las mediciones','4 de noviembre de 2026']],
 [139,CW-139],caption='Entorno de las pruebas. Fuente: entorno.json.')
p('Las mediciones corresponden a este computador. No fijamos un núcleo específico '
  'para cada hilo ni controlamos la temperatura, la frecuencia efectiva o todas '
  'las tareas de Windows. Por eso los tiempos pueden variar y no deben tomarse '
  'como una promesa de desempeño para cualquier equipo.')
sub('7.2 Benchmark o comparación repetida de tiempos')
p('El benchmark consistió en ejecutar las variantes varias veces y comparar '
  'cuánto tardaban. Usamos 100 mil, un millón y diez millones de lanzamientos. '
  'Para cada algoritmo, tamaño y compilación probamos once configuraciones: '
  'serial, buffered, OpenMP static con cinco cantidades de hilos, SIMD static '
  'con uno y doce hilos, y OpenMP con doce hilos usando dynamic y guided.')
formula('3 compilaciones × 2 algoritmos × 3 tamaños × 11 configuraciones × 5 repeticiones')
formula('Total de la campaña: 990 mediciones')
p('El script benchmark.py ejecutó los procesos uno por uno en un orden mezclado '
  'con una semilla fija. Antes de medir hicimos calentamientos. Todas las '
  'repeticiones usaron la semilla 42 para conservar los mismos lanzamientos. '
  'Así medimos la variación de tiempo con un trabajo igual en cada caso.')
sub('7.3 Qué tiempo y qué memoria registramos')
p('Medimos por separado el cálculo y la ejecución completa del proceso. El '
  'primer tiempo incluye generar las muestras, evaluarlas y sumar los conteos, '
  'después de preparar el equipo de hilos. No incluye abrir el proceso ni '
  'imprimir la respuesta. El segundo sí incluye toda la ejecución, y permite '
  'ver si el arranque pesa demasiado cuando la simulación es pequeña.')
p('En Windows usamos QueryPerformanceCounter, un reloj de alta resolución [6]. '
  'Descartamos una campaña inicial porque el reloj de OpenMP devolvió algunos '
  'tiempos cero para trabajos pequeños, y repetimos las mediciones con el nuevo '
  'reloj. También registramos el tiempo CPU del proceso, aunque su resolución '
  'no alcanzó para distinguir bien algunos casos cortos.')
p('Para cada configuración publicamos la mediana de cinco tiempos: el valor '
  'que queda en la mitad después de ordenarlos. Conservamos también el menor, '
  'el mayor y una medida de su dispersión. La aceleración, llamada speedup, '
  'indica cuántas veces se reduce el tiempo respecto del serial del mismo '
  'programa. La eficiencia divide esa aceleración entre los hilos utilizados.')
formula('Aceleración = tiempo serial / tiempo paralelo')
p('La memoria registrada es el máximo uso de memoria residente del proceso '
  'completo, que incluye el programa y sus bibliotecas. Medimos también cuántos '
  'bytes ocupan únicamente los arreglos. No medimos los fallos de caché ni '
  'la velocidad de transferencia de memoria del hardware.')

h('8 Pruebas para comprobar que el cálculo seguía bien')
sub('8.1 Las 163 comprobaciones')
table(['Prueba','Casos','Qué revisamos'],[
 ['Referencia en Python','108','Mismos conteos que una implementación separada de las fórmulas.'],
 ['Cambios de reparto','36','Mismo conteo con static, dynamic y guided.'],
 ['Datos de entrada incorrectos','7','Rechazo de tamaños, hilos o valores fuera del rango permitido.'],
 ['Varias semillas','10','Aproximaciones coherentes con el error aleatorio esperado.'],
 ['Programas originales','2','Compilación, ejecución y revisión de sus aproximaciones.'],
 ['Total','163','Todas las comprobaciones pasaron.']],
 [122,40,CW-162],total=True,caption='Pruebas de verify.py. Evidencia conservada en verificacion.json.')
p('Para la referencia usamos 1, 17 y 10003 lanzamientos, con semillas distintas. '
  'Comparamos los modos serial, buffered, OpenMP con uno, dos y cuatro hilos '
  'y SIMD con cuatro hilos, en las tres compilaciones. En las pruebas de '
  'reparto usamos 100003 lanzamientos y cuatro hilos. Esos tamaños permiten '
  'comprobar también qué ocurre cuando la cantidad no se divide exactamente '
  'entre los hilos.')
p('Los siete casos incorrectos incluyeron cero o un número negativo de '
  'muestras, cero hilos, un algoritmo inexistente, pedir varios hilos en '
  'serial y valores enteros demasiado grandes. Para las pruebas con varias '
  'semillas usamos un millón de muestras y un margen amplio de seis errores '
  'estándar, que tiene en cuenta la variación esperada por el azar. Eso '
  'comprueba coherencia; no exige decimales idénticos entre semillas.')
sub('8.2 Ejemplo con diez millones de muestras')
correct=[]
for algo in ('dartboard','needles'):
    r=next(r for r in data.RAW if r['algorithm']==algo and r['n']=='10000000' and r['mode']=='serial' and r['variant']=='native')
    correct.append([algo.capitalize(),r['hits'],data.num(r['pi'],12),data.num(r['abs_error'],10)])
table(['Algoritmo','Aciertos o cruces','π estimado','Error absoluto'],correct,
      [74,105,163,CW-342],caption='Semilla 42. Estos conteos coinciden entre los modos comparados.')
p('Además de las 163 comprobaciones, el benchmark revisó durante sus 990 '
  'ejecuciones que el conteo no cambiara para un mismo algoritmo, tamaño '
  'y semilla. Esta igualdad exacta era importante: una versión más rápida '
  'no sería una mejora si perdiera lanzamientos o sumara mal.')

h('9 Resultados de la comparación de tiempos')
sub('9.1 Efecto de las opciones de compilación')
rows=[]
for v in ('O2','O3','native'):
    rows.append([v,data.ms(data.row('dartboard',variant=v)),data.ms(data.row('needles',variant=v))])
table(['Compilación','Dartboard en ms','Needles en ms'],rows,[110,180,CW-290],
      caption='Versiones seriales con diez millones de muestras. Cada valor es la mediana de cinco ejecuciones.')
p('O3 redujo el tiempo frente a O2 aproximadamente 1,34 veces en Dartboard '
  'y 1,03 veces en Needles. La opción native no fue mejor de forma constante. '
  'Cuando las diferencias son pequeñas y los tiempos varían, conviene '
  'evitar conclusiones demasiado firmes sobre una opción del compilador.')
sub('9.2 Efecto de repartir entre más hilos')
p('La siguiente tabla usa la compilación native y diez millones de muestras. '
  'Los tiempos están en milisegundos, es decir, milésimas de segundo. '
  'Cada aceleración se calcula frente al serial de ese mismo algoritmo.')
rows=[]
for mode,pn,schedule,label in [('serial',1,'static','Serial'),('omp',2,'static','2 hilos static'),
        ('omp',4,'static','4 hilos static'),('omp',6,'static','6 hilos static'),
        ('omp',12,'static','12 hilos static'),('omp',12,'dynamic','12 hilos dynamic'),
        ('omp',12,'guided','12 hilos guided')]:
    d=data.row('dartboard',mode,pn,schedule=schedule);n=data.row('needles',mode,pn,schedule=schedule)
    rows.append([label,data.ms(d),data.num(d['speedup'],2)+'×',data.ms(n),data.num(n['speedup'],2)+'×'])
table(['Configuración','Dartboard ms','Aceleración','Needles ms','Aceleración'],rows,
      [121,91,83,92,CW-387],caption='Comparación con igual compilación, tamaño y semilla.')
p('Ambos algoritmos redujeron su tiempo con más hilos. Needles obtuvo '
  'mayor aceleración relativa porque tiene más trabajo por lanzamiento '
  'y puede repartirlo entre los hilos. Aun así, Dartboard terminó mucho '
  'antes en tiempo total. Pasar de seis a doce hilos tampoco duplicó '
  'la mejora: hay organización del trabajo, esperas y recursos compartidos.')
figure('speedup.png','Figura 1. Aceleración con diez millones de muestras y compilación native. La línea discontinua representa la mejora ideal proporcional a los hilos. Fuente: resumen.csv.')
sub('9.3 El menor tiempo y la mayor aceleración pueden ser distintos')
rows=[]
for a,v in [('dartboard','O2'),('dartboard','native'),('needles','O3'),('needles','native')]:
    s=data.row(a,variant=v);r=data.row(a,'omp',12,variant=v,schedule='dynamic')
    rows.append([a.capitalize(),v,data.ms(s),data.ms(r),data.num(r['speedup'],2)+'×'])
table(['Algoritmo','Compilación','Serial ms','Paralelo ms','Aceleración'],rows,
      [82,83,98,104,CW-367],caption='Diez millones de muestras y doce hilos con dynamic. Cada fila tiene su propia referencia serial.')
p('Dartboard native tuvo el menor tiempo observado: 5,646 ms, con '
  'una aceleración de 7,06 veces. Dartboard O2 tuvo una aceleración '
  'mayor, 8,76 veces, porque su versión serial tardaba más. Por eso '
  'una cifra mayor de speedup no siempre significa un menor tiempo final.')
p('En Needles, O3 obtuvo el menor tiempo observado: 41,266 ms frente '
  'a 420,053 ms de su serial. La aceleración fue de 10,18 veces y '
  'la eficiencia de 84,8 % al dividirla entre doce hilos. Estas son '
  'las mejores medianas encontradas en la campaña, no resultados '
  'garantizados para otra ejecución o computador.')
sub('9.4 Por qué también medimos la ejecución completa')
p('Con 100 mil muestras, Dartboard serial native tardó 0,327 ms '
  'en el cálculo, pero 10,550 ms como proceso completo. Con doce '
  'hilos y static, el cálculo bajó a 0,137 ms y el proceso completo '
  'tardó 12,387 ms. En ese caso, abrir y preparar el trabajo pesa '
  'más que lo ahorrado en el cálculo. Por eso OpenMP puede ser '
  'útil en simulaciones grandes y no compensar en una ejecución pequeña.')

h('10 Perfilado y medición de la memoria')
sub('10.1 Qué parte del programa tardó más')
p('El benchmark nos dice cuánto tarda cada versión. El perfilado '
  'busca explicar en qué parte se va ese tiempo. Primero intentamos '
  'usar gprof, una herramienta que registra el trabajo de las funciones. '
  'Compilamos la versión preparada para esa herramienta y la ejecutamos, '
  'pero en este entorno sus archivos no contenían muestras útiles.')
p('Para obtener evidencia real, medimos por separado dos partes de '
  'buffered: generar y guardar las muestras, y leerlas para evaluar '
  'si acertaban. Usamos treinta millones de lanzamientos, descartamos '
  'un calentamiento y tomamos cinco ejecuciones por algoritmo. '
  'Esta forma de medir se llama perfilado por instrumentación: '
  'colocar relojes alrededor de las partes que queremos estudiar.')
rows=[]
for r in data.PROFILE['instrumented_phases']:
    rows.append([r['algorithm'].capitalize(),data.num(r['median_generation_s']*1000),
                 data.num(r['median_evaluation_s']*1000),data.num(r['evaluation_fraction_of_phases']*100,1)+' %'])
table(['Algoritmo','Generar y guardar ms','Leer y evaluar ms','Evaluación respecto de ambas fases'],rows,
      [74,122,117,CW-313],caption='Perfilado de buffered con treinta millones de muestras. Fuente: perfil_fases.csv y perfil_estado.json.')
p('En Dartboard tardó más generar y guardar los datos. En Needles '
  'la evaluación representó cerca del 89 % de la suma de esas '
  'dos fases. Es coherente con el cálculo de seno por lanzamiento, '
  'pero no significa que hayamos medido un 89 % exclusivamente '
  'dentro de sin: la fase también incluye lecturas y el conteo. '
  'Estos porcentajes corresponden a buffered y no se trasladan '
  'directamente a la versión que no guarda arreglos.')
sub('10.2 Qué mostró la prueba de memoria')
rows=[]
for a in ('dartboard','needles'):
    for mode,label in [('serial','Sin arreglos'),('buffered','Con arreglos')]:
        r=data.row(a,mode)
        rows.append([a.capitalize(),label,data.ms(r),data.num(float(r['median_peak_rss_kib'])/1024,2)])
table(['Algoritmo','Versión','Tiempo ms','Pico de memoria MiB'],rows,
      [82,121,103,CW-306],caption='Diez millones de muestras, compilación native y un hilo. Pico de memoria del proceso completo.')
p('Guardar las muestras elevó la memoria de unos 4,1 a 156,7 MiB. '
  'La diferencia concuerda con los dos arreglos de 152,59 MiB '
  'adicionales. La versión sin arreglos fue cerca de 1,85 veces '
  'más rápida en Dartboard y 1,06 veces en Needles. En Needles '
  'el cálculo geométrico pesa más, de modo que el almacenamiento '
  'representa una parte menor del tiempo.')
figure('memoria_perfil.png','Figura 2. Comparación de memoria y tiempos por partes del programa. El gráfico de memoria usa diez millones de muestras; el perfilado usa treinta millones. Fuente: mediciones del proyecto.')
sub('10.3 Qué pasó con SIMD')
table(['Algoritmo','OpenMP normal ms','OpenMP con SIMD ms'],[
 ['Dartboard',data.ms(data.row('dartboard','omp',12)),data.ms(data.row('dartboard','simd',12))],
 ['Needles',data.ms(data.row('needles','omp',12)),data.ms(data.row('needles','simd',12))]],
 [95,181,CW-276],caption='Doce hilos, static, native y diez millones de muestras.')
p('Dartboard no mejoró con SIMD en esta comparación. En Needles '
  'la diferencia fue pequeña frente a la variación de los tiempos. '
  'Los mensajes del compilador también mostraron que el ciclo '
  'completo de muestras no se transformó de forma efectiva en '
  'operaciones vectoriales. Por eso no presentamos SIMD como '
  'una mejora confirmada en este trabajo.')

h('11 Explicación de los resultados y sus límites')
sub('11.1 Repartir más trabajo puede aprovechar mejor los hilos')
p('Needles tarda más por lanzamiento. Cuando repartimos muchos '
  'lanzamientos, los hilos tienen suficiente trabajo útil para '
  'compensar parte del costo de organización. Esto ayuda a '
  'entender su mayor aceleración relativa. Dartboard hace '
  'menos trabajo por muestra y termina antes, aunque su '
  'aceleración frente al serial sea menor.')
sub('11.2 La forma de reparto y Windows también influyen')
p('Con doce hilos, Needles native y static tuvo tiempos entre '
  '44,065 y 68,801 ms; con dynamic, entre 39,999 y 44,676 ms. '
  'El reparto flexible pudo compensar hilos que avanzaron '
  'más lentamente, pero no medimos por separado todas las '
  'causas. El sistema operativo y otras tareas también '
  'pueden alterar el ritmo de ejecución.')
p('En algunos casos de pocos hilos, la eficiencia calculada '
  'superó el 100 %. Eso no demuestra que cada hilo rindiera '
  'más de su capacidad física. La versión serial y la '
  'región OpenMP se compilan por caminos diferentes y '
  'también hay variación temporal. Por ejemplo, Dartboard '
  'con OpenMP de un hilo ya fue más rápido que su serial.')
sub('11.3 Qué podemos afirmar con estas pruebas')
p('Las pruebas respaldan que los conteos se conservaron en '
  'los casos revisados, que OpenMP redujo los tiempos de '
  'las simulaciones grandes y que guardar las muestras '
  'aumentó la memoria y el tiempo. No bastan para afirmar '
  'que una compilación o una forma de reparto siempre '
  'será superior. Tampoco medimos por separado el costo '
  'de cada instrucción ni los efectos internos de la caché.')
p('Para profundizar sería útil repetir en otros equipos, '
  'asignar los hilos de forma controlada a los núcleos '
  'y medir en condiciones similares de energía y carga. '
  'También convendría usar una herramienta de perfilado '
  'de funciones que sí produzca datos útiles en el '
  'entorno y evaluar el generador con pruebas especializadas.')

h('12 Conclusiones')
for text in [
 'Los programas originales son seriales y sus fórmulas son correctas para las condiciones usadas. Esto permitió continuar desde una base revisada.',
 'OpenMP permitió repartir los lanzamientos y sumar los conteos sin cambiar el resultado. Pasaron las 163 comprobaciones y los controles de las 990 mediciones.',
 'Con diez millones de muestras, los menores tiempos observados fueron 5,646 ms en Dartboard native y 41,266 ms en Needles O3, con doce hilos y dynamic.',
 'El mayor speedup de Dartboard fue 8,76 con O2, mientras native tuvo el menor tiempo y un speedup de 7,06. En Needles O3, el speedup fue 10,18.',
 'O3 ayudó especialmente a Dartboard. Las opciones native y SIMD no mejoraron siempre, de modo que fue necesario probarlas y no darlas por superiores.',
 'Generar y usar cada muestra de inmediato mantuvo bajo el uso de memoria. Guardarlas todas elevó el pico de unos 4,1 a 156,7 MiB y aumentó el tiempo.',
 'El perfilado por fases mostró un peso mayor de la evaluación en Needles. gprof no produjo datos útiles y esa limitación se conservó en el informe.',
 'En estas condiciones, Dartboard fue más rápido y tiene menor variación esperada por muestra. Las conclusiones deben revisarse al cambiar el equipo o las condiciones de ejecución.'
]:p('• '+text)

doc.add_page_break()
h('13 Bibliografía y fuentes')
for text,url in data.refs:
    q=p(text+'\n'+url)
    q.alignment=WD_ALIGN_PARAGRAPH.LEFT
    for run in q.runs:run.font.size=Pt(11)
p('[7] Archivos del proyecto y resultados experimentales del 4 de noviembre de 2026. '
  'Fuentes: Needle.c, Dartboard.c y montecarlo.c. Scripts: build.py, verify.py, '
  'benchmark.py, profile.py y graficas.py. Resultados: mediciones.csv, resumen.csv, '
  'verificacion.json, entorno.json, perfil_fases.csv, perfil_estado.json y vector_*.txt.')

doc.add_page_break()
h('Anexo A Código y comandos para repetir el trabajo')
sub('A1 Cálculo serial de los programas originales')
p('Estos fragmentos muestran las operaciones centrales. En Dartboard contamos '
  'puntos dentro del círculo; en Needles contamos agujas que cruzan. Los archivos '
  'originales completos también incluyen la preparación y la impresión de resultados.')
code('''/* Dartboard */
for (long long i = 0; i < N; i++) {
    x = 2.0 * rand() / RAND_MAX - 1.0;
    y = 2.0 * rand() / RAND_MAX - 1.0;
    if (x * x + y * y <= 1.0) dentro++;
}
pi = 4.0 * (double)dentro / N;''')
code('''/* Needles, con L=D=1 */
for (long long i = 0; i < N; i++) {
    distancia = ((double)rand() / RAND_MAX) * (D / 2.0);
    angulo = ((double)rand() / RAND_MAX) * (PI / 2.0);
    if (distancia <= (L / 2.0) * sin(angulo)) cruces++;
}
if (cruces > 0) pi = (2.0 * L * N) / (D * cruces);''')
sub('A2 Cómo repetir las pruebas')
code('''cd "E:\\HPC\\Reto 2"
python build.py
python verify.py
python profile.py
python benchmark.py --repeats 5
python graficas.py''')
p('El perfilado y el benchmark se ejecutan por separado. Los scripts usan '
  'Python y GCC con OpenMP; graficas.py necesita matplotlib. Ejemplos '
  'de ejecución directa para diez millones de muestras y semilla 42:')
code('''.\\build\\mc_native.exe dartboard serial 10000000 1 42 static
.\\build\\mc_native.exe dartboard omp 10000000 12 42 dynamic
.\\build\\mc_O3.exe needles serial 10000000 1 42 static
.\\build\\mc_O3.exe needles omp 10000000 12 42 dynamic''')
sub('A3 Opciones técnicas que se mantuvieron iguales')
p('Las compilaciones nuevas usan -fopenmp para habilitar OpenMP, '
  '-ffp-contract=off para evitar ciertas diferencias por operaciones '
  'aritméticas fusionadas y -fno-math-errno porque el programa no '
  'consulta ese indicador después de sin. No se activó -ffast-math. '
  'La compilación para intentar gprof usó -O2 -g -pg -fno-inline '
  'y no se mezcló con los tiempos del benchmark. Los comandos '
  'completos se conservan en compilacion.txt.')

doc.add_page_break()
h('Anexo B Cinco repeticiones de las configuraciones más rápidas')
p('Estas son las cinco mediciones de las configuraciones con menor '
  'tiempo central para diez millones de muestras. Los archivos '
  'conservan las 990 ejecuciones completas; no elegimos una sola '
  'repetición favorable para publicar el resultado.')
for a,v,label in [('dartboard','native','Dartboard native'),('needles','O3','Needles O3')]:
    sub(label)
    values=sorted([r for r in data.RAW if r['algorithm']==a and r['variant']==v and
       r['n']=='10000000' and r['mode']=='omp' and r['threads_actual']=='12' and
       r['schedule']=='dynamic'],key=lambda r:int(r['repeat']))
    rows=[[r['repeat'],data.num(float(r['wall_s'])*1000,4),r['hits'],
           data.num(float(r['process_wall_s'])*1000,4)] for r in values]
    r=data.row(a,'omp',12,variant=v,schedule='dynamic')
    rows.append(['Mediana',data.ms(r),'Mismo conteo',data.num(float(r['median_process_wall_s'])*1000,4)])
    table(['Repetición','Cálculo ms','Aciertos o cruces','Proceso completo ms'],rows,
          [75,108,127,CW-310],total=True,
          caption=label+', doce hilos y dynamic. Fuente: mediciones.csv.')

target=OUT/NAME
doc.save(target)
# Revision estructural: el texto y las tablas son elementos editables de Word.
check=Document(target)
text='\n'.join(q.text for q in check.paragraphs)+'\n'+'\n'.join(c.text for t in check.tables for r in t.rows for c in r.cells)
for term in ['163','990','8,76','10,18','156,7','Ramiro Andrés Barrios Valencia']:
    assert term in text,term
assert len(check.tables)>=12
assert len(check.inline_shapes)==2
print(target)
print('Parrafos:',len(check.paragraphs),'Tablas editables:',len(check.tables),'Graficas:',len(check.inline_shapes))
