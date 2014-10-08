
#undef NDEBUG

#include "ospray/common/OspCommon.h"
#include "apps/common/xml/xml.h"
#include "model.h"

#include "common/sys/filename.h"

namespace ospray {
  namespace particle {
    bool big_endian = false;

    struct Particle {
      double x,y,z;
    };
    // struct MPM {
    //   std::vector<vec3f> pos;
    // };
    // MPM mpm;

    double htonlf(double f)
    {
      double ret;
      char *in = (char*)&f;
      char *out = (char*)&ret;
      for (int i=0;i<8;i++)
        out[i] = in[7-i];
      return ret;
    }
    void readParticles(Model *model,
                       size_t numParticles, const std::string &fn, size_t begin, size_t end)
    {
      // std::cout << "#mpm: reading " << numParticles << " particles... " << std::flush;
      // printf("#mpm: reading%7ld particles...",numParticles); fflush(0);
      FILE *file = fopen(fn.c_str(),"rb");
      if (!file) {
        throw std::runtime_error("could not open data file "+fn);
      }
      assert(file);

      fseek(file,begin,SEEK_SET);
      size_t len = end-begin;

      if (len != numParticles*sizeof(Particle)) {
        PING;
        PRINT(len);
        PRINT(numParticles);
        PRINT(len/numParticles);
      }
      // PRINT(len);
      
      for (int i=0;i<numParticles;i++) {
        Particle p;
        int rc = fread(&p,sizeof(p),1,file);
        if (rc != 1) {
          fclose(file);
          throw std::runtime_error("read partial data "+fn);
        }
#if 1
        if (big_endian) {
          p.x = htonlf(p.x);
          p.y = htonlf(p.y);
          p.z = htonlf(p.z);
        }
#endif
        Model::Atom a;
        a.position = vec3f(p.x,p.y,p.z);
        a.type = model->getAtomType("<unnamed>");
        model->atom.push_back(a);
      }
      
      std::cout << "\r#osp:uintah: read " << numParticles << " particles (total " << float(model->atom.size()/1e6) << "M)";

      // Particle *particle = new Particle[numParticles];
      // fread(particle,numParticles,sizeof(Particle),file);
      // for (int i=0;i
      // for (int i=0;i<100;i++)
      //   printf("particle %5i: %lf %lf %lf\n",particle[i].x,particle[i].y,particle[i].z);
      fclose(file);
    }

    void parse__Variable(Model *model,
                       const std::string &basePath, xml::Node *var)
    {
      size_t index = -1;
      size_t start = -1;
      size_t end = -1;
      size_t patch = -1;
      size_t numParticles = 0;
      std::string variable;
      std::string filename;
      for (int i=0;i<var->child.size();i++) {
        xml::Node *n = var->child[i];
        if (n->name == "index") {
          index = atoi(n->content.c_str());
        } else if (n->name == "variable") {
          variable = n->content;
        } else if (n->name == "numParticles") {
          numParticles = atol(n->content.c_str());
        } else if (n->name == "patch") {
          patch = atol(n->content.c_str());
        } else if (n->name == "filename") {
          filename = n->content;
        } else if (n->name == "start") {
          start = atol(n->content.c_str());
        } else if (n->name == "end") {
          end = atol(n->content.c_str());
        }
      }

      if (numParticles > 0
          && variable == "p.x"
          /* && index == .... */ 
          ) {
        readParticles(model,numParticles,basePath+"/"+filename,start,end);
      }
      // PRINT(patch);
      // PRINT(numParticles);
      // PRINT(start);
      // PRINT(filename);
    }

    // pase a "Uintah_Output" node
    // void parse__Uintah_Output(const std::string &basePath, xml::Node *node)
    // {
    //   assert(node->name == "Uintah_Output");
    //   for (int i=0;i<node->child.size();i++) {
    //     xml::Node *c = node->child[i];
    //     assert(c->name == "Variable");
    //     parse__Variable(basePath,c);
    //   }
    // }
    void parse__Uintah_Datafile(Model *model,
                       const std::string &fileName)
    {
      std::string basePath = embree::FileName(fileName).path();

      xml::XMLDoc *doc = NULL;
      try {
        doc = xml::readXML(fileName);
      } catch (std::runtime_error e) {
        static bool warned = false;
        if (!warned) {
          std::cerr << "#osp:uintah: error in opening xml data file: " << e.what() << std::endl;
           std::cerr << "#osp:uintah: continuing parsing, but parts of the data will be missing" << std::endl;
           std::cerr << "#osp:uintah: (only printing first instance of this error; there may be more)" << std::endl;
           warned = true;
        }
        return;
      }
      assert(doc);
      assert(doc->child.size() == 1);
      xml::Node *node = doc->child[0];
      assert(node->name == "Uintah_Output");
      for (int i=0;i<node->child.size();i++) {
        xml::Node *c = node->child[i];
        assert(c->name == "Variable");
        parse__Variable(model,basePath,c);
      }
      delete doc;
    }
    void parse__Uintah_TimeStep_Data(Model *model,
                                     const std::string &basePath, xml::Node *node)
    {
      assert(node->name == "Data");
      for (int i=0;i<node->child.size();i++) {
        xml::Node *c = node->child[i];
        assert(c->name == "Datafile");
        for (int j=0;j<c->prop.size();j++) {
          xml::Prop *p = c->prop[j];
          if (p->name == "href") {
            try {
              parse__Uintah_Datafile(model,basePath+"/"+p->value);
            } catch (std::runtime_error e) {
              static bool warned = false;
              if (!warned) {
                std::cerr << "#osp:uintah: error in parsing timestep data: " << e.what() << std::endl;
                std::cerr << "#osp:uintah: continuing parsing, but parts of the data will be missing" << std::endl;
                std::cerr << "#osp:uintah: (only printing first instance of this error; there may be more)" << std::endl;
                warned = true;
              }
            }
          }
        }
      }
    }
    void parse__Uintah_TimeStep_Meta(Model *model,
                                     const std::string &basePath, xml::Node *node)
    {
      assert(node->name == "Meta");
      for (int i=0;i<node->child.size();i++) {
        xml::Node *c = node->child[i];
        if (c->name == "endianness") {
          if (c->content == "big_endian") {
            std::cout << "#osp:uintah: SWITCHING TO BIG_ENDIANNESS" << std::endl;
            big_endian = true;
          }
        }
      }
    }
    void parse__Uintah_timestep(Model *model,
                                const std::string &basePath, xml::Node *node)
    {
      assert(node->name == "Uintah_timestep");
      for (int i=0;i<node->child.size();i++) {
        xml::Node *c = node->child[i];
        if (c->name == "Meta") {
          parse__Uintah_TimeStep_Meta(model,basePath,c);
        }
        if (c->name == "Data") {
          parse__Uintah_TimeStep_Data(model,basePath,c);
        }
      }
    }
    Model *parse__Uintah_timestep_xml(const std::string &s)
    {
      Model *model = new Model;
      Ref<xml::XMLDoc> doc = xml::readXML(s);
      assert(doc);
      assert(doc->child.size() == 1);
      assert(doc->child[0]->name == "Uintah_timestep");
      std::string basePath = embree::FileName(s).path();
      parse__Uintah_timestep(model, basePath, doc->child[0]);
      std::cout << "#osp:mpm: read " << s << " : " 
                << model->atom.size() << " particles" << std::endl;

      box3f bounds = embree::empty;
      for (int i=0;i<model->atom.size();i++) {
        bounds.extend(model->atom[i].position);
      }
      std::cout << "#osp:mpm: bounds of particle centers: " << bounds << std::endl;
      model->radius = .002f;
      return model;
    }
  }
}

