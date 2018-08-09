using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;

namespace WebTranslator.Models
{
    public class DataPackage
    {
        
        public string FileName { get; set; }
        public string FileContents { get; set; }
        public string EntryName { get; set; }
        public string Shader { get; set; }
        public string Language { get; set; }
        public string Result { get; set; }

    }
}
