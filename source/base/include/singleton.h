/** Portable implementation of the singleton design pattern. Used extensively. **/
#ifndef SGFBASE_SINGLETON_H_
#define SGFBASE_SINGLETON_H_

template<typename T>
class Singleton
{
  private:
    static T* singletonObject;

  public:
    explicit Singleton();

    virtual ~Singleton();

    static T* getInstance();

};
template<typename T>
T* Singleton<T>::singletonObject = NULL;

template<typename T>
Singleton<T>::Singleton()
{
	VERIFY(singletonObject == NULL);
	singletonObject = static_cast<T*>(this); // this should be portable
}

template<typename T>
Singleton<T>::~Singleton()
{
	VERIFY(singletonObject != NULL);
	delete singletonObject;
}

template<typename T>
T* Singleton<T>::getInstance()
{
	if(singletonObject == NULL) { new T(); }
	return singletonObject;
}

#endif /* SGFBASE_SINGLETON_H_ */
